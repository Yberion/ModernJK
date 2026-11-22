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

// cvar.c -- dynamic variable tracking

#include "q_shared.h"
#include "qcommon.h"

cvar_t* cvar_vars = nullptr;
cvar_t* cvar_cheats;
cvarFlags_t cvar_modifiedFlags;

constexpr int MAX_CVARS = 8192;
cvar_t cvar_indexes[MAX_CVARS];
int cvar_numIndexes;

constexpr int FILE_HASH_SIZE = 512;
static cvar_t* hashTable[FILE_HASH_SIZE];
static bool cvar_sort = false;

static char* lastMemPool = nullptr;
static int memPoolSize;

// If the string came from the memory pool, don't really free it.  The entire
// memory pool will be wiped during the next level load.
static void Cvar_FreeString(char* string)
{
    if (!lastMemPool || string < lastMemPool || string >= lastMemPool + memPoolSize) {
        Z_Free(string);
    }
}

/*
================
return a hash value for the filename
================
*/
static long generateHashValue(const char* fname)
{
    int i;
    long hash;
    char letter;

    hash = 0;
    i = 0;
    while (fname[i] != '\0') {
        letter = tolower((unsigned char)fname[i]);
        hash += (long)(letter) * (i + 119);
        i++;
    }
    hash &= (FILE_HASH_SIZE - 1);
    return hash;
}

/*
============
Cvar_ValidateString
============
*/
static bool Cvar_ValidateString(const char* s)
{
    if (!s) {
        return false;
    }
    if (strchr(s, '\\')) {
        return false;
    }
    if (strchr(s, '\"')) {
        return false;
    }
    if (strchr(s, ';')) {
        return false;
    }
    return true;
}

/*
============
Cvar_FindVar
============
*/
static cvar_t* Cvar_FindVar(const char* var_name)
{
    cvar_t* var;
    long hash;

    hash = generateHashValue(var_name);

    for (var = hashTable[hash]; var; var = var->hashNext) {
        if (!Q_stricmp(var_name, var->name)) {
            return var;
        }
    }

    return nullptr;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue(const char* var_name)
{
    cvar_t* var;

    var = Cvar_FindVar(var_name);
    if (!var)
        return 0;
    return var->value;
}

/*
============
Cvar_VariableIntegerValue
============
*/
int Cvar_VariableIntegerValue(const char* var_name)
{
    cvar_t* var;

    var = Cvar_FindVar(var_name);
    if (!var)
        return 0;
    return var->integer;
}

/*
============
Cvar_VariableString
============
*/
const char* Cvar_VariableString(const char* var_name)
{
    cvar_t* var;

    var = Cvar_FindVar(var_name);
    if (!var)
        return "";
    return var->string;
}

/*
============
Cvar_VariableStringBuffer
============
*/
void Cvar_VariableStringBuffer(const char* var_name, char* buffer, int bufsize)
{
    cvar_t* var;

    var = Cvar_FindVar(var_name);
    if (!var) {
        *buffer = 0;
    }
    else {
        Q_strncpyz(buffer, var->string, bufsize);
    }
}

/*
============
Cvar_Flags
============
*/
std::optional<cvarFlags_t> Cvar_Flags(const char* var_name)
{
    cvar_t* var;

    if (!(var = Cvar_FindVar(var_name)))
        return std::nullopt;
    else {
        if (var->modified)
            return var->flags | cvarFlags_t::CVAR_MODIFIED;
        else
            return var->flags;
    }
}

/*
============
Cvar_CommandCompletion
============
*/
void Cvar_CommandCompletion(callbackFunc_t callback)
{
    cvar_t* cvar;

    for (cvar = cvar_vars; cvar; cvar = cvar->next) {
        if (Any(cvar->flags & cvarFlags_t::CVAR_CHEAT) && !cvar_cheats->integer) {
            continue;
        }
        callback(cvar->name);
    }
}

/*
============
Cvar_Validate
============
*/
static const char* Cvar_Validate(cvar_t* var, const char* value, bool warn)
{
    char warning[MAX_STRING_CHARS] = ""; // the warning, built in pieces, printed with the value set
    static char s[MAX_CVAR_VALUE_STRING];
    float valuef;
    bool changed = false;

    if (!var->validate)
        return value;

    if (!value)
        return value;

    if (Q_isanumber(value)) {
        valuef = Q_atof(value);

        if (var->integral) {
            if (!Q_isintegral(valuef)) {
                if (warn)
                    Q_FormatTo(warning, "cvar '{}' must be integral", var->name);

                valuef = (int)valuef;
                changed = true;
            }
        }
    }
    else {
        if (warn)
            Q_FormatTo(warning, "cvar '{}' must be numeric", var->name);

        valuef = Q_atof(var->resetString);
        changed = true;
    }

    if (valuef < var->min) {
        if (warn) {
            if (changed)
                Q_strcat(warning, sizeof(warning), " and is");
            else
                Q_FormatTo(warning, "cvar '{}'", var->name);

            if (Q_isintegral(var->min))
                Q_strcat(warning, sizeof(warning), Q_Format(" out of range (min {})", (int)var->min).c_str());
            else
                Q_strcat(warning, sizeof(warning), Q_Format(" out of range (min {:f})", var->min).c_str());
        }

        valuef = var->min;
        changed = true;
    }
    else if (valuef > var->max) {
        if (warn) {
            if (changed)
                Q_strcat(warning, sizeof(warning), " and is");
            else
                Q_FormatTo(warning, "cvar '{}'", var->name);

            if (Q_isintegral(var->max))
                Q_strcat(warning, sizeof(warning), Q_Format(" out of range (max {})", (int)var->max).c_str());
            else
                Q_strcat(warning, sizeof(warning), Q_Format(" out of range (max {:f})", var->max).c_str());
        }

        valuef = var->max;
        changed = true;
    }

    if (changed) {
        if (Q_isintegral(valuef)) {
            Q_FormatTo(s, "{}", (int)valuef);

            if (warn)
                Q_PrintlnWarning("{}, setting to {}", warning, (int)valuef);
        }
        else {
            Q_FormatTo(s, "{:f}", valuef);

            if (warn)
                Q_PrintlnWarning("{}, setting to {:f}", warning, valuef);
        }

        return s;
    }
    else
        return value;
}

/*
============
Cvar_Get

If the variable already exists, the value will not be set unless CVAR_ROM
The flags will be or'ed in if the variable exists.
============
*/
cvar_t* Cvar_Get(const char* var_name, const char* var_value, cvarFlags_t flags)
{
    cvar_t* var;
    long hash;
    int index;

    if (!var_name || !var_value) {
        Q_Error(errorParm_t::ERR_FATAL, "Cvar_Get: NULL parameter");
    }

    if (!Cvar_ValidateString(var_name)) {
        Q_Println("invalid cvar name string: {}", var_name);
        var_name = "BADNAME";
    }

#if 0 // FIXME: values with backslash happen
	if ( !Cvar_ValidateString( var_value ) ) {
		Q_Println("invalid cvar value string: {}", var_value);
		var_value = "BADVALUE";
	}
#endif

    var = Cvar_FindVar(var_name);
    if (var) {
        var_value = Cvar_Validate(var, var_value, false);

        // Make sure the game code cannot mark engine-added variables as gamecode vars
        if (Any(var->flags & cvarFlags_t::CVAR_VM_CREATED)) {
            if (!Any(flags & cvarFlags_t::CVAR_VM_CREATED))
                var->flags &= ~cvarFlags_t::CVAR_VM_CREATED;
        }
        else if (!Any(var->flags & cvarFlags_t::CVAR_USER_CREATED)) {
            if (Any(flags & cvarFlags_t::CVAR_VM_CREATED))
                flags &= ~cvarFlags_t::CVAR_VM_CREATED;
        }

        // if the C code is now specifying a variable that the user already
        // set a value for, take the new value as the reset value
        if (Any(var->flags & cvarFlags_t::CVAR_USER_CREATED)) {
            var->flags &= ~cvarFlags_t::CVAR_USER_CREATED;
            Cvar_FreeString(var->resetString);
            var->resetString = CopyString(var_value);

            if (Any(flags & cvarFlags_t::CVAR_ROM)) {
                // this variable was set by the user,
                // so force it to value given by the engine.

                if (var->latchedString)
                    Cvar_FreeString(var->latchedString);

                var->latchedString = CopyString(var_value);
            }
        }

        // Make sure servers cannot mark engine-added variables as SERVER_CREATED
        if (Any(var->flags & cvarFlags_t::CVAR_SERVER_CREATED)) {
            if (!Any(flags & cvarFlags_t::CVAR_SERVER_CREATED))
                var->flags &= ~cvarFlags_t::CVAR_SERVER_CREATED;
        }
        else {
            if (Any(flags & cvarFlags_t::CVAR_SERVER_CREATED))
                flags &= ~cvarFlags_t::CVAR_SERVER_CREATED;
        }

        var->flags |= flags;

        // only allow one non-empty reset string without a warning
        if (!var->resetString[0]) {
            // we don't have a reset string yet
            Cvar_FreeString(var->resetString);
            var->resetString = CopyString(var_value);
        }
        else if (var_value[0] && strcmp(var->resetString, var_value)) {
            Q_DPrintln(S_COLOR_YELLOW "Warning: cvar \"{}\" given initial values: \"{}\" and \"{}\"", var_name, var->resetString, var_value);
        }
        // if we have a latched string, take that value now
        if (var->latchedString) {
            char* s;

            s = var->latchedString;
            var->latchedString = nullptr; // otherwise cvar_set2 would free it
            Cvar_Set2(var_name, s, true);
            Cvar_FreeString(s);
        }

        // ZOID--needs to be set so that cvars the game sets as
        // SERVERINFO get sent to clients
        cvar_modifiedFlags |= flags;

        return var;
    }

    //
    // allocate a new cvar
    //

    // find a free cvar
    for (index = 0; index < MAX_CVARS; index++) {
        if (!cvar_indexes[index].name)
            break;
    }

    if (index >= MAX_CVARS) {
        if (!com_errorEntered)
            Q_Error(errorParm_t::ERR_FATAL, "Error: Too many cvars, cannot create a new one!");

        return nullptr;
    }

    var = &cvar_indexes[index];

    if (index >= cvar_numIndexes)
        cvar_numIndexes = index + 1;

    var->name = CopyString(var_name);
    var->string = CopyString(var_value);
    var->modified = true;
    var->modificationCount = 1;
    var->value = Q_atof(var->string);
    var->integer = atoi(var->string);
    var->resetString = CopyString(var_value);
    var->validate = false;

    // link the variable in
    var->next = cvar_vars;
    if (cvar_vars)
        cvar_vars->prev = var;

    var->prev = nullptr;
    cvar_vars = var;

    var->flags = flags;
    // note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
    cvar_modifiedFlags |= var->flags;

    hash = generateHashValue(var_name);
    var->hashIndex = hash;

    var->hashNext = hashTable[hash];
    if (hashTable[hash])
        hashTable[hash]->hashPrev = var;

    var->hashPrev = nullptr;
    hashTable[hash] = var;

    // sort on write
    cvar_sort = true;

    return var;
}

static void Cvar_QSortByName(cvar_t** a, int n)
{
    cvar_t* temp;
    cvar_t* m;
    int i, j;

    i = 0;
    j = n;
    m = a[n >> 1];

    do {
        // sort in descending order
        while (strcmp(a[i]->name, m->name) > 0)
            i++;
        while (strcmp(a[j]->name, m->name) < 0)
            j--;

        if (i <= j) {
            temp = a[i];
            a[i] = a[j];
            a[j] = temp;
            i++;
            j--;
        }
    } while (i <= j);

    if (j > 0)
        Cvar_QSortByName(a, j);
    if (n > i)
        Cvar_QSortByName(a + i, n - i);
}

static void Cvar_Sort(void)
{
    cvar_t *list[MAX_CVARS], *var;
    int count;
    int i;

    for (count = 0, var = cvar_vars; var; var = var->next) {
        if (var->name) {
            list[count++] = var;
        }
        else {
            Q_Error(errorParm_t::ERR_FATAL, "Cvar_Sort: NULL cvar name");
        }
    }

    if (count < 2) {
        return; // nothing to sort
    }

    Cvar_QSortByName(&list[0], count - 1);

    cvar_vars = nullptr;

    // relink cvars
    for (i = 0; i < count; i++) {
        var = list[i];
        // link the variable in
        var->next = cvar_vars;
        if (cvar_vars)
            cvar_vars->prev = var;
        var->prev = nullptr;
        cvar_vars = var;
    }
}

/*
============
Cvar_Print

Prints the value, default, and latched string of the given variable
============
*/
void Cvar_Print(cvar_t* v)
{
    Q_Print(S_COLOR_GREY "Cvar " S_COLOR_WHITE "{} = " S_COLOR_GREY "\"" S_COLOR_WHITE "{}" S_COLOR_GREY "\"" S_COLOR_WHITE, v->name, v->string);

    if (!Any(v->flags & cvarFlags_t::CVAR_ROM)) {
        if (!Q_stricmp(v->string, v->resetString))
            Q_Print(", " S_COLOR_WHITE "the default");
        else
            Q_Print(", " S_COLOR_WHITE "default = " S_COLOR_GREY "\"" S_COLOR_WHITE "{}" S_COLOR_GREY "\"" S_COLOR_WHITE, v->resetString);
    }

    Q_Println("");

    if (v->latchedString)
        Q_Println("     latched = " S_COLOR_GREY "\"" S_COLOR_WHITE "{}" S_COLOR_GREY "\"", v->latchedString);
}

/*
============
Cvar_Set2
============
*/
cvar_t* Cvar_Set2(const char* var_name, const char* value, bool force)
{
    cvar_t* var;

    // Q_DPrintln("Cvar_Set2: {} {}", var_name, value);

    if (!Cvar_ValidateString(var_name)) {
        Q_Println("invalid cvar name string: {}", var_name);
        var_name = "BADNAME";
    }

#if 0 // FIXME
	if ( value && !Cvar_ValidateString( value ) ) {
		Q_Println("invalid cvar value string: {}", value);
		var_value = "BADVALUE";
	}
#endif

    var = Cvar_FindVar(var_name);
    if (!var) {
        if (!value) {
            return nullptr;
        }
        // create it
        if (!force) {
            return Cvar_Get(var_name, value, cvarFlags_t::CVAR_USER_CREATED);
        }
        else {
            return Cvar_Get(var_name, value, cvarFlags_t::CVAR_NONE);
        }
    }

    if (!value) {
        value = var->resetString;
    }

    value = Cvar_Validate(var, value, true);

    if (Any(var->flags & cvarFlags_t::CVAR_LATCH) && var->latchedString) {
        if (!strcmp(value, var->string)) {
            Cvar_FreeString(var->latchedString);
            var->latchedString = nullptr;
            return var;
        }

        if (!strcmp(value, var->latchedString))
            return var;
    }
    else if (!strcmp(value, var->string))
        return var;

    // note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
    cvar_modifiedFlags |= var->flags;

    if (!force) {
        if (Any(var->flags & cvarFlags_t::CVAR_ROM)) {
            Q_Println("{} is read only.", var_name);
            return var;
        }

        if (Any(var->flags & cvarFlags_t::CVAR_INIT)) {
            Q_Println("{} is write protected.", var_name);
            return var;
        }

        if (Any(var->flags & cvarFlags_t::CVAR_LATCH)) {
            if (var->latchedString) {
                if (strcmp(value, var->latchedString) == 0)
                    return var;
                Cvar_FreeString(var->latchedString);
            }
            else {
                if (strcmp(value, var->string) == 0)
                    return var;
            }

            Q_Println("{} will be changed upon restarting.", var_name);
            var->latchedString = CopyString(value);
            var->modified = true;
            var->modificationCount++;
            return var;
        }

        if (Any(var->flags & cvarFlags_t::CVAR_CHEAT) && !cvar_cheats->integer) {
            Q_Println("{} is cheat protected.", var_name);
            return var;
        }
    }
    else {
        if (var->latchedString) {
            Cvar_FreeString(var->latchedString);
            var->latchedString = nullptr;
        }
    }

    if (!strcmp(value, var->string))
        return var; // not changed

    var->modified = true;
    var->modificationCount++;

    Cvar_FreeString(var->string); // free the old value string

    var->string = CopyString(value);
    var->value = Q_atof(var->string);
    var->integer = atoi(var->string);

    return var;
}

/*
============
Cvar_Set
============
*/
void Cvar_Set(const char* var_name, const char* value)
{
    Cvar_Set2(var_name, value, true);
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue(const char* var_name, float value)
{
    char val[32];

    if (value == (int)value) {
        Q_FormatTo(val, "{}", (int)value);
    }
    else {
        Q_FormatTo(val, "{:f}", value);
    }
    Cvar_Set(var_name, val);
}

/*
============
Cvar_SetValue2
============
*/
void Cvar_SetValue2(const char* var_name, float value, bool force)
{
    char val[32];

    if (Q_isintegral(value))
        Q_FormatTo(val, "{}", (int)value);
    else
        Q_FormatTo(val, "{:f}", value);
    Cvar_Set2(var_name, val, force);
}

/*
============
Cvar_Reset
============
*/
void Cvar_Reset(const char* var_name)
{
    Cvar_Set2(var_name, nullptr, false);
}

/*
============
Cvar_ForceReset
============
*/
void Cvar_ForceReset(const char* var_name)
{
    Cvar_Set2(var_name, nullptr, true);
}

/*
============
Cvar_SetCheatState

Any testing variables will be reset to the safe values
============
*/
void Cvar_SetCheatState(void)
{
    cvar_t* var;

    // set all default vars to the safe value
    for (var = cvar_vars; var; var = var->next) {
        if (Any(var->flags & cvarFlags_t::CVAR_CHEAT)) {
            // the CVAR_LATCHED|CVAR_CHEAT vars might escape the reset here
            // because of a different var->latchedString
            if (var->latchedString) {
                Cvar_FreeString(var->latchedString);
                var->latchedString = nullptr;
            }
            if (strcmp(var->resetString, var->string)) {
                Cvar_Set(var->name, var->resetString);
            }
        }
    }
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
bool Cvar_Command(void)
{
    cvar_t* v;

    // check variables
    v = Cvar_FindVar(Cmd_Argv(0));
    if (!v) {
        return false;
    }

    // perform a variable print or set
    if (Cmd_Argc() == 1) {
        Cvar_Print(v);
        return true;
    }

    // toggle
    if (!strcmp(Cmd_Argv(1), "!")) {
        // Swap the value if our command has ! in it (bind p "cg_thirdPeson !")
        Cvar_SetValue2(v->name, !v->value, false);
        return true;
    }

    // set the value if forcing isn't required
    Cvar_Set2(v->name, Cmd_Args(), false);

    return true;
}

/*
============
Cvar_Print_f

Prints the contents of a cvar
(preferred over Cvar_Command where cvar names and commands conflict)
============
*/
void Cvar_Print_f(void)
{
    const char* name;
    cvar_t* cv;

    if (Cmd_Argc() != 2) {
        Q_Println("usage: print <variable>");
        return;
    }

    name = Cmd_Argv(1);

    cv = Cvar_FindVar(name);

    if (cv)
        Cvar_Print(cv);
    else
        Q_Println("Cvar {} does not exist.", name);
}

/*
============
Cvar_Toggle_f

Toggles a cvar for easy single key binding, optionally through a list of
given values
============
*/
void Cvar_Toggle_f(void)
{
    int i, c = Cmd_Argc();
    const char* curval;

    if (c < 2) {
        Q_Println("usage: toggle <variable> [value1, value2, ...]");
        return;
    }

    if (c == 2) {
        Cvar_Set2(Cmd_Argv(1), Q_Format("{:d}", !Cvar_VariableValue(Cmd_Argv(1))).c_str(), false);
        return;
    }

    if (c == 3) {
        Q_Println("toggle: nothing to toggle to");
        return;
    }

    curval = Cvar_VariableString(Cmd_Argv(1));

    // don't bother checking the last arg for a match since the desired
    // behaviour is the same as no match (set to the first argument)
    for (i = 2; i + 1 < c; i++) {
        if (strcmp(curval, Cmd_Argv(i)) == 0) {
            Cvar_Set2(Cmd_Argv(1), Cmd_Argv(i + 1), false);
            return;
        }
    }

    // fallback
    Cvar_Set2(Cmd_Argv(1), Cmd_Argv(2), false);
}

/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console, even if they
weren't declared in C code.
============
*/
void Cvar_Set_f(void)
{
    int c;
    const char* cmd;
    cvar_t* v;

    c = Cmd_Argc();
    cmd = Cmd_Argv(0);

    if (c < 2) {
        Q_Println("usage: {} <variable> <value>", cmd);
        return;
    }
    if (c == 2) {
        Cvar_Print_f();
        return;
    }

    v = Cvar_Set2(Cmd_Argv(1), Cmd_ArgsFrom(2), false);
    if (!v) {
        return;
    }
    switch (cmd[3]) {
    case 'a':
        if (!Any(v->flags & cvarFlags_t::CVAR_ARCHIVE)) {
            v->flags |= cvarFlags_t::CVAR_ARCHIVE;
            cvar_modifiedFlags |= cvarFlags_t::CVAR_ARCHIVE;
        }
        break;
    case 'u':
        if (!Any(v->flags & cvarFlags_t::CVAR_USERINFO)) {
            v->flags |= cvarFlags_t::CVAR_USERINFO;
            cvar_modifiedFlags |= cvarFlags_t::CVAR_USERINFO;
        }
        break;
    case 's':
        if (!Any(v->flags & cvarFlags_t::CVAR_SERVERINFO)) {
            v->flags |= cvarFlags_t::CVAR_SERVERINFO;
            cvar_modifiedFlags |= cvarFlags_t::CVAR_SERVERINFO;
        }
        break;
    }
}

/*
============
Cvar_Reset_f
============
*/
void Cvar_Reset_f(void)
{
    if (Cmd_Argc() != 2) {
        Q_Println("usage: reset <variable>");
        return;
    }
    Cvar_Reset(Cmd_Argv(1));
}

/*
============
Cvar_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables(fileHandle_t f)
{
    cvar_t* var;
    char buffer[1024];

    if (cvar_sort) {
        Q_DPrintln("Cvar_Sort: sort cvars");
        cvar_sort = false;
        Cvar_Sort();
    }

    for (var = cvar_vars; var; var = var->next) {
        if (!var->name || Q_stricmp(var->name, "cl_cdkey") == 0)
            continue;

        if (Any(var->flags & cvarFlags_t::CVAR_ARCHIVE)) {
            // write the latched value, even if it hasn't taken effect yet
            if (var->latchedString) {
                if (strlen(var->name) + strlen(var->latchedString) + 10 > sizeof(buffer)) {
                    Q_PrintlnWarning("value of variable \"{}\" too long to write to file", var->name);
                    continue;
                }
                if (Any(var->flags & cvarFlags_t::CVAR_NODEFAULT) && !strcmp(var->latchedString, var->resetString)) {
                    continue;
                }
                Q_FormatTo(buffer, "seta {} \"{}\"\n", var->name, var->latchedString);
            }
            else {
                if (strlen(var->name) + strlen(var->string) + 10 > sizeof(buffer)) {
                    Q_PrintlnWarning("value of variable \"{}\" too long to write to file", var->name);
                    continue;
                }
                if (Any(var->flags & cvarFlags_t::CVAR_NODEFAULT) && !strcmp(var->string, var->resetString)) {
                    continue;
                }
                Q_FormatTo(buffer, "seta {} \"{}\"\n", var->name, var->string);
            }
            FS_Write(buffer, strlen(buffer), f);
        }
    }
}

/*
============
Cvar_List_f
============
*/
void Cvar_List_f(void)
{
    cvar_t* var = nullptr;
    int i = 0;
    const char* match = nullptr;

    if (Cmd_Argc() > 1)
        match = Cmd_Argv(1);

    for (var = cvar_vars, i = 0; var; var = var->next, i++) {
        if (!var->name || (match && !Com_Filter(match, var->name, false)))
            continue;

        if (Any(var->flags & cvarFlags_t::CVAR_SERVERINFO))
            Q_Print("S");
        else
            Q_Print(" ");
        if (Any(var->flags & cvarFlags_t::CVAR_SYSTEMINFO))
            Q_Print("s");
        else
            Q_Print(" ");
        if (Any(var->flags & cvarFlags_t::CVAR_USERINFO))
            Q_Print("U");
        else
            Q_Print(" ");
        if (Any(var->flags & cvarFlags_t::CVAR_ROM))
            Q_Print("R");
        else
            Q_Print(" ");
        if (Any(var->flags & cvarFlags_t::CVAR_INIT))
            Q_Print("I");
        else
            Q_Print(" ");
        if (Any(var->flags & cvarFlags_t::CVAR_ARCHIVE))
            Q_Print("A");
        else
            Q_Print(" ");
        if (Any(var->flags & cvarFlags_t::CVAR_LATCH))
            Q_Print("L");
        else
            Q_Print(" ");
        if (Any(var->flags & cvarFlags_t::CVAR_CHEAT))
            Q_Print("C");
        else
            Q_Print(" ");
        if (Any(var->flags & cvarFlags_t::CVAR_USER_CREATED))
            Q_Print("?");
        else
            Q_Print(" ");

        Q_Print(S_COLOR_WHITE " {} = " S_COLOR_GREY "\"" S_COLOR_WHITE "{}" S_COLOR_GREY "\"" S_COLOR_WHITE, var->name, var->string);
        if (var->latchedString)
            Q_Print(", latched = " S_COLOR_GREY "\"" S_COLOR_WHITE "{}" S_COLOR_GREY "\"" S_COLOR_WHITE, var->latchedString);
        Q_Println("");
    }

    Q_Println("\n{} total cvars", i);
    if (i != cvar_numIndexes)
        Q_Println("{} cvar indexes", cvar_numIndexes);
}

void Cvar_ListModified_f(void)
{
    cvar_t* var = nullptr;

    // build a list of cvars that are modified
    for (var = cvar_vars; var; var = var->next) {
        char* value = var->latchedString ? var->latchedString : var->string;
        if (!var->name || !var->modificationCount || !strcmp(value, var->resetString))
            continue;

        Q_Println(S_COLOR_GREY "Cvar " S_COLOR_WHITE "{} = " S_COLOR_GREY "\"" S_COLOR_WHITE "{}" S_COLOR_GREY "\"" S_COLOR_WHITE ", " S_COLOR_WHITE
                               "default = " S_COLOR_GREY "\"" S_COLOR_WHITE "{}" S_COLOR_GREY "\"" S_COLOR_WHITE,
                  var->name, value, var->resetString);
    }
}

void Cvar_ListUserCreated_f(void)
{
    cvar_t* var = nullptr;
    uint32_t count = 0;

    // build a list of cvars that are modified
    for (var = cvar_vars; var; var = var->next) {
        char* value = var->latchedString ? var->latchedString : var->string;
        if (!Any(var->flags & cvarFlags_t::CVAR_USER_CREATED))
            continue;

        Q_Println(S_COLOR_GREY "Cvar " S_COLOR_WHITE "{} = " S_COLOR_GREY "\"" S_COLOR_WHITE "{}" S_COLOR_GREY "\"" S_COLOR_WHITE, var->name, value);
        count++;
    }

    if (count > 0)
        Q_Println(S_COLOR_GREY "Showing " S_COLOR_WHITE "{}" S_COLOR_GREY " user created cvars" S_COLOR_WHITE, count);
    else
        Q_Println(S_COLOR_GREY "No user created cvars" S_COLOR_WHITE);
}

/*
============
Cvar_Unset

Unsets a cvar
============
*/

cvar_t* Cvar_Unset(cvar_t* cv)
{
    cvar_t* next = cv->next;

    // note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
    cvar_modifiedFlags |= cv->flags;

    if (cv->name)
        Cvar_FreeString(cv->name);
    if (cv->string)
        Cvar_FreeString(cv->string);
    if (cv->latchedString)
        Cvar_FreeString(cv->latchedString);
    if (cv->resetString)
        Cvar_FreeString(cv->resetString);

    if (cv->prev)
        cv->prev->next = cv->next;
    else
        cvar_vars = cv->next;
    if (cv->next)
        cv->next->prev = cv->prev;

    if (cv->hashPrev)
        cv->hashPrev->hashNext = cv->hashNext;
    else
        hashTable[cv->hashIndex] = cv->hashNext;
    if (cv->hashNext)
        cv->hashNext->hashPrev = cv->hashPrev;

    memset(cv, 0, sizeof(*cv));

    return next;
}

/*
============
Cvar_Unset_f

Unsets a userdefined cvar
============
*/

void Cvar_Unset_f(void)
{
    cvar_t* cv;

    if (Cmd_Argc() != 2) {
        Q_Println("Usage: {} <varname>", Cmd_Argv(0));
        return;
    }

    cv = Cvar_FindVar(Cmd_Argv(1));

    if (!cv)
        return;

    if (Any(cv->flags & cvarFlags_t::CVAR_USER_CREATED))
        Cvar_Unset(cv);
    else
        Q_PrintlnError("{}: Variable {} is not user created.", Cmd_Argv(0), cv->name);
}

void Cvar_UnsetUserCreated_f(void)
{
    cvar_t* curvar = cvar_vars;
    uint32_t count = 0;

    while (curvar) {
        if (Any(curvar->flags & cvarFlags_t::CVAR_USER_CREATED)) {
            // throw out any variables the user created
            curvar = Cvar_Unset(curvar);
            count++;
            continue;
        }
        curvar = curvar->next;
    }

    if (count > 0)
        Q_Println(S_COLOR_GREY "Removed " S_COLOR_WHITE "{}" S_COLOR_GREY " user created cvars" S_COLOR_WHITE, count);
    else
        Q_Println(S_COLOR_GREY "No user created cvars to remove" S_COLOR_WHITE);
}

/*
============
Cvar_Restart

Resets all cvars to their hardcoded values and removes userdefined variables
and variables added via the VMs if requested.
============
*/

void Cvar_Restart(bool unsetVM)
{
    cvar_t* curvar;

    curvar = cvar_vars;

    while (curvar) {
        if (Any(curvar->flags & cvarFlags_t::CVAR_USER_CREATED) || (unsetVM && Any(curvar->flags & cvarFlags_t::CVAR_VM_CREATED))) {
            // throw out any variables the user/vm created
            curvar = Cvar_Unset(curvar);
            continue;
        }

        if (!Any(curvar->flags & (cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INIT | cvarFlags_t::CVAR_NORESTART))) {
            // Just reset the rest to their default values.
            Cvar_Set2(curvar->name, curvar->resetString, false);
        }

        curvar = curvar->next;
    }
}

/*
============
Cvar_Restart_f

Resets all cvars to their hardcoded values
============
*/
void Cvar_Restart_f(void)
{
    Cvar_Restart(false);
}

/*
=====================
Cvar_InfoString
=====================
*/
char* Cvar_InfoString(cvarFlags_t bit)
{
    static char info[MAX_INFO_STRING];
    cvar_t* var;

    info[0] = 0;

    for (var = cvar_vars; var; var = var->next) {
        if (var->name && Any(var->flags & bit)) {
            Info_SetValueForKey(info, var->name, var->string);
        }
    }
    return info;
}

/*
=====================
Cvar_InfoStringBuffer
=====================
*/
void Cvar_InfoStringBuffer(cvarFlags_t bit, char* buff, int buffsize)
{
    Q_strncpyz(buff, Cvar_InfoString(bit), buffsize);
}

/*
=====================
Cvar_CheckRange
=====================
*/
void Cvar_CheckRange(cvar_t* var, float min, float max, bool integral)
{
    var->validate = true;
    var->min = min;
    var->max = max;
    var->integral = integral;

    // Force an initial range check
    Cvar_Set(var->name, var->string);
}

/*
=====================
Cvar_Register

basically a slightly modified Cvar_Get for the interpreted modules
=====================
*/
void Cvar_Register(vmCvar_t* vmCvar, const char* varName, const char* defaultValue, cvarFlags_t flags)
{
    cvar_t* cv;

    // There is code in Cvar_Get to prevent CVAR_ROM cvars being changed by the
    // user. In other words CVAR_ARCHIVE and CVAR_ROM are mutually exclusive
    // flags. Unfortunately some historical game code (including single player
    // baseq3) sets both flags. We unset CVAR_ROM for such cvars.
    if ((flags & (cvarFlags_t::CVAR_ARCHIVE | cvarFlags_t::CVAR_ROM)) == (cvarFlags_t::CVAR_ARCHIVE | cvarFlags_t::CVAR_ROM)) {
        Q_DPrintln(S_COLOR_YELLOW "WARNING: Unsetting CVAR_ROM cvar '{}', since it is also CVAR_ARCHIVE", varName);
        flags &= ~cvarFlags_t::CVAR_ROM;
    }

    cv = Cvar_Get(varName, defaultValue, flags | cvarFlags_t::CVAR_VM_CREATED);
    if (!vmCvar) {
        return;
    }
    vmCvar->handle = cv - cvar_indexes;
    vmCvar->modificationCount = -1;
    Cvar_Update(vmCvar);
}

/*
=====================
Cvar_Update

updates an interpreted modules' version of a cvar
=====================
*/
void Cvar_Update(vmCvar_t* vmCvar)
{
    cvar_t* cv = nullptr;
    assert(vmCvar);

    if ((unsigned)vmCvar->handle >= (unsigned)cvar_numIndexes) {
        Q_Error(errorParm_t::ERR_DROP, "Cvar_Update: handle {} out of range", (unsigned)vmCvar->handle);
    }

    cv = cvar_indexes + vmCvar->handle;

    if (cv->modificationCount == vmCvar->modificationCount) {
        return;
    }
    if (!cv->string) {
        return; // variable might have been cleared by a cvar_restart
    }
    vmCvar->modificationCount = cv->modificationCount;
    if (strlen(cv->string) + 1 > MAX_CVAR_VALUE_STRING)
        Q_Error(errorParm_t::ERR_DROP, "Cvar_Update: src {} length {} exceeds MAX_CVAR_VALUE_STRING", cv->string, (unsigned int)strlen(cv->string));
    Q_strncpyz(vmCvar->string, cv->string, MAX_CVAR_VALUE_STRING);
    vmCvar->value = cv->value;
    vmCvar->integer = cv->integer;
}

/*
==================
Cvar_CompleteCvarName
==================
*/
void Cvar_CompleteCvarName(char* args, int argNum)
{
    if (argNum == 2) {
        // Skip "<cmd> "
        char* p = Com_SkipTokens(args, 1, " ");

        if (p > args)
            Field_CompleteCommand(p, false, true);
    }
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init(void)
{
    memset(cvar_indexes, 0, sizeof(cvar_indexes));
    memset(hashTable, 0, sizeof(hashTable));

    cvar_cheats = Cvar_Get("helpUsObi", "0", cvarFlags_t::CVAR_SYSTEMINFO);

    Cmd_AddCommand("print", Cvar_Print_f);
    Cmd_SetCommandCompletionFunc("print", Cvar_CompleteCvarName);
    Cmd_AddCommand("toggle", Cvar_Toggle_f);
    Cmd_SetCommandCompletionFunc("toggle", Cvar_CompleteCvarName);
    Cmd_AddCommand("set", Cvar_Set_f);
    Cmd_SetCommandCompletionFunc("set", Cvar_CompleteCvarName);
    Cmd_AddCommand("sets", Cvar_Set_f);
    Cmd_SetCommandCompletionFunc("sets", Cvar_CompleteCvarName);
    Cmd_AddCommand("setu", Cvar_Set_f);
    Cmd_SetCommandCompletionFunc("setu", Cvar_CompleteCvarName);
    Cmd_AddCommand("seta", Cvar_Set_f);
    Cmd_SetCommandCompletionFunc("seta", Cvar_CompleteCvarName);
    Cmd_AddCommand("reset", Cvar_Reset_f);
    Cmd_SetCommandCompletionFunc("reset", Cvar_CompleteCvarName);
    Cmd_AddCommand("unset", Cvar_Unset_f);
    Cmd_SetCommandCompletionFunc("unset", Cvar_CompleteCvarName);
    Cmd_AddCommand("unset_usercreated", Cvar_UnsetUserCreated_f);
    Cmd_AddCommand("cvarlist", Cvar_List_f);
    Cmd_AddCommand("cvar_usercreated", Cvar_ListUserCreated_f);
    Cmd_AddCommand("cvar_modified", Cvar_ListModified_f);
    Cmd_AddCommand("cvar_restart", Cvar_Restart_f);
}

static void Cvar_Realloc(char** string, char* memPool, int& memPoolUsed)
{
    if (string && *string) {
        char* temp = memPool + memPoolUsed;
        strcpy(temp, *string);
        memPoolUsed += strlen(*string) + 1;
        Cvar_FreeString(*string);
        *string = temp;
    }
}

// Turns many small allocation blocks into one big one.
void Cvar_Defrag(void)
{
    cvar_t* var;
    int totalMem = 0;
    int nextMemPoolSize;

    for (var = cvar_vars; var; var = var->next) {
        if (var->name) {
            totalMem += strlen(var->name) + 1;
        }
        if (var->string) {
            totalMem += strlen(var->string) + 1;
        }
        if (var->resetString) {
            totalMem += strlen(var->resetString) + 1;
        }
        if (var->latchedString) {
            totalMem += strlen(var->latchedString) + 1;
        }
    }

    char* mem = (char*)Z_Malloc(totalMem, memtag::TAG_SMALL, false);
    nextMemPoolSize = totalMem;
    totalMem = 0;

    for (var = cvar_vars; var; var = var->next) {
        Cvar_Realloc(&var->name, mem, totalMem);
        Cvar_Realloc(&var->string, mem, totalMem);
        Cvar_Realloc(&var->resetString, mem, totalMem);
        Cvar_Realloc(&var->latchedString, mem, totalMem);
    }

    if (lastMemPool) {
        Z_Free(lastMemPool);
    }
    lastMemPool = mem;
    memPoolSize = nextMemPoolSize;
}

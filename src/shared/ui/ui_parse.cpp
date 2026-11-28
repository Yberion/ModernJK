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

// ui_parse.cpp -- the menus' reader, the same in the three games and compiled by each menus' module against its own
// headers (the multiplayer's UI and cgame, the single player's engine): a menu file read whole and split by
// COM_ParseExt, the numbers, texts, colours, rectangles and scripts its keywords take, the names of the game's
// menudef.h read where a number is, a menu file's top level and a list of menu files. Each tree reads a file
// (UI_ReadMenuFile); each module reads its assets (Asset_Parse) and its menus' keywords (Menu_New).

#include <ui/ui_shared.h>

#include <algorithm>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

// the menu file being read
struct menuParse_t {
    char fileName[MAX_QPATH];
    // the file, then a second end of text: after a quote left open at the file's end, COM_ParseExt steps over the first
    std::string text;
    const char* current; // where the reading is; null at the file's end, or when no file is open
    bool open;           // a file is being read: its parse session begun
};

menuParse_t menuReading;

// a name of the game's menudef.h: its value as an integer and as a float
struct menuName_t {
    int integer;
    float real;
};

struct menuNameHash_t {
    using is_transparent = void;
    size_t operator()(std::string_view name) const noexcept { return std::hash<std::string_view>{}(name); }
};

std::unordered_map<std::string, menuName_t, menuNameHash_t, std::equal_to<>> menuNames;
bool menuNamesRead = false; // menudef.h read once at least: by a list of menu files, else by the first menu file

// the line of the place read, counted in the text
int PC_Line()
{
    const char* start = menuReading.text.c_str();
    const char* end = menuReading.current ? menuReading.current : start + menuReading.text.size();
    return 1 + static_cast<int>(std::count(start, end, '\n'));
}

// a #define line's value: one decimal or hexadecimal number
std::optional<menuName_t> PC_NameValue(std::string_view value)
{
    const char* first = value.data();
    const char* last = first + value.size();

    if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        unsigned int number = 0;
        const auto [end, error] = std::from_chars(first + 2, last, number, 16);
        if (error != std::errc() || end != last) {
            return std::nullopt;
        }
        return menuName_t{static_cast<int>(number), static_cast<float>(number)};
    }

    int number = 0;
    if (const auto [end, error] = std::from_chars(first, last, number); error == std::errc() && end == last) {
        return menuName_t{number, static_cast<float>(number)};
    }

    const std::string text(value);
    const char* end = nullptr;
    const float real = Q_strtof(text.c_str(), &end);
    if (end == text.c_str() || end != text.c_str() + text.size()) {
        return std::nullopt;
    }
    return menuName_t{static_cast<int>(real), real};
}

// the names of the game's menudef.h, from its "#define NAME value" lines whose value is one number; its other lines
// (the colours of three numbers, the voice chat's quoted words) are left out, no menu using them
void PC_LoadMenuNames()
{
    menuNames.clear();
    menuNamesRead = true;

    std::string text;
    if (!UI_ReadMenuFile(MENUDEF_FILE, text)) {
        Q_PrintlnWarning("{} not found: the menus' names are not read", MENUDEF_FILE);
        return;
    }

    std::string_view rest(text);
    while (!rest.empty()) {
        const size_t lineEnd = rest.find('\n');
        std::string_view line = rest.substr(0, lineEnd);
        rest = (lineEnd == std::string_view::npos) ? std::string_view() : rest.substr(lineEnd + 1);

        line = line.substr(0, std::min(line.find("//"), line.find("/*")));

        // the line's words, at the spaces
        std::string_view words[4];
        int count = 0;
        size_t start = line.find_first_not_of(" \t\r");
        while (start != std::string_view::npos && count < 4) {
            const size_t stop = line.find_first_of(" \t\r", start);
            words[count++] = line.substr(start, stop - start);
            start = line.find_first_not_of(" \t\r", stop);
        }
        if (start != std::string_view::npos) {
            continue; // more than four words: not one value
        }

        int first = 0;
        if (count >= 1 && words[0] == "#") {
            first = 1;
        }
        else if (count >= 1 && words[0] == "#define") {
            words[0] = "define";
        }
        if (count - first != 3 || words[first] != "define") {
            continue;
        }
        if (const std::optional<menuName_t> value = PC_NameValue(words[first + 2])) {
            menuNames.insert_or_assign(std::string(words[first + 1]), *value);
        }
    }
}

// a token read where a number is, on its keyword's line or the next ones (the multiplayer's menus write a list box's
// columns on several lines, which botlib's precompiler read); null at the file's end or for an empty quoted string
const char* PC_NumberToken()
{
    const char* token = PC_ParseExt();
    return (token && token[0]) ? token : nullptr;
}

// the value of a token that is a word: the menudef.h name's, or 0 with a warning for a word it does not define; none
// for a number
std::optional<menuName_t> PC_NameOf(const char* token)
{
    const char c = token[0];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')) {
        return std::nullopt;
    }

    const auto found = menuNames.find(std::string_view(token));
    if (found != menuNames.end()) {
        return found->second;
    }
    PC_ParseWarning(Q_Format("Unknown name '{}', read as 0", token).c_str());
    return menuName_t{0, 0.0f};
}

// a script's token appended to its text: quoted unless it is one character
void PC_AppendScriptToken(char (&script)[4096], std::string_view token)
{
    if (token.size() == 1) {
        Q_strcat(script, sizeof(script), Q_Format("{} ", token).c_str());
    }
    else {
        Q_strcat(script, sizeof(script), Q_Format("\"{}\" ", token).c_str());
    }
}

// a loadMenu block of a list of menu files: { "file" ... }, each file read in turn
bool Load_Menu(const char** text)
{
    const char* token = COM_ParseExt(text, true);
    if (*token != '{') {
        return false;
    }

    while (true) {
        token = COM_ParseExt(text, true);
        if (!token[0]) {
            return false;
        }
        if (*token == '}') {
            return true;
        }
        UI_ParseMenu(token);
    }
}

} // namespace

bool PC_StartParseSession(const char* fileName)
{
    Q_strncpyz(menuReading.fileName, fileName, sizeof(menuReading.fileName));
    menuReading.current = nullptr;

    // the single player's cgame reads its HUD's menu files without a list
    if (!menuNamesRead) {
        PC_LoadMenuNames();
    }
    if (!UI_ReadMenuFile(menuReading.fileName, menuReading.text) || menuReading.text.empty()) {
        return false;
    }
    menuReading.text.push_back('\0');
    menuReading.current = menuReading.text.c_str();
    menuReading.open = true;
    COM_BeginParseSession(menuReading.fileName);
    return true;
}

void PC_EndParseSession()
{
    // the single player's cgame ends a session it could not begin
    if (!menuReading.open) {
        return;
    }
    COM_EndParseSession();
    menuReading.open = false;
    menuReading.current = nullptr;
    menuReading.text.clear();
}

void PC_ParseWarning(const char* message)
{
    Q_PrintlnWarning("{} Line #{} of File '{}'", message, PC_Line(), menuReading.fileName);
}

const char* PC_ParseExt()
{
    if (!menuReading.current) {
        return nullptr;
    }
    const char* token = COM_ParseExt(&menuReading.current, true);
    return menuReading.current ? token : nullptr;
}

bool PC_ParseString(const char** string)
{
    const char* token = PC_ParseExt();
    if (!token) {
        return false;
    }
    *string = token;
    return true;
}

bool PC_ParseStringMem(const char** out)
{
    const char* token;
    if (!PC_ParseString(&token)) {
        return false;
    }
    *out = String_Alloc(token);
    return true;
}

bool PC_ParseInt(int* number)
{
    const char* token = PC_NumberToken();
    if (!token) {
        return false;
    }
    if (const std::optional<menuName_t> name = PC_NameOf(token)) {
        *number = name->integer;
    }
    else {
        *number = atoi(token);
    }
    return true;
}

bool PC_ParseFloat(float* number)
{
    const char* token = PC_NumberToken();
    if (!token) {
        return false;
    }
    if (const std::optional<menuName_t> name = PC_NameOf(token)) {
        *number = name->real;
    }
    else {
        *number = static_cast<float>(Q_atof(token));
    }
    return true;
}

bool PC_ParseColor(vec4_t* color)
{
    for (int i = 0; i < 4; i++) {
        if (!PC_ParseFloat(&(*color)[i])) {
            return false;
        }
    }
    return true;
}

bool PC_ParseRect(rectDef_t* r)
{
    return PC_ParseFloat(&r->x) && PC_ParseFloat(&r->y) && PC_ParseFloat(&r->w) && PC_ParseFloat(&r->h);
}

bool PC_Script_Parse(const char** out)
{
    char script[4096];
    script[0] = 0;

    // scripts start with { and have ; separated command lists.. commands are command, arg..
    // basically we want everything between the { } as it will be interpreted at run time
    const char* token = PC_ParseExt();
    if (!token || *token != '{') {
        return false;
    }

    while (true) {
        token = PC_ParseExt();
        if (!token) {
            return false;
        }

        if (*token == '}') { // End of the script?
            *out = String_Alloc(script);
            return true;
        }

        // a ';' against a word without quotes ends a command, as botlib's precompiler read the multiplayer's menus
        std::string_view rest(token);
        const bool quoted = menuReading.current[-1] == '"';
        if (quoted || rest.size() == 1) {
            PC_AppendScriptToken(script, rest);
            continue;
        }
        while (!rest.empty()) {
            const size_t separator = rest.find(';');
            if (separator != 0) {
                PC_AppendScriptToken(script, rest.substr(0, separator));
            }
            if (separator == std::string_view::npos) {
                break;
            }
            PC_AppendScriptToken(script, ";");
            rest.remove_prefix(separator + 1);
        }
    }
}

void UI_ParseMenu(const char* menuFile)
{
    if (!PC_StartParseSession(menuFile)) {
        Q_Println("UI_ParseMenu: Unable to load menu {}", menuReading.fileName);
        return;
    }

    while (true) {
        const char* token = PC_ParseExt();
        if (!token || *token == '}') {
            break;
        }
        if (*token == '{') {
            continue;
        }
        if (Q_stricmp(token, "assetGlobalDef") == 0) {
            if (Asset_Parse()) {
                continue;
            }
            break;
        }
        if (Q_stricmp(token, "menudef") == 0) {
            Menu_New();
            continue;
        }
        // a word outside a menu, which some of the multiplayer's files have at their top
        Q_DPrintln("Invalid keyword '{}' Line #{} of File '{}'", token, PC_Line(), menuReading.fileName);
    }

    PC_EndParseSession();
}

void UI_ParseMenuList(const char* listFile, const char* text)
{
    PC_LoadMenuNames();
    COM_BeginParseSession(listFile);

    const char* p = text;
    while (true) {
        const char* token = COM_ParseExt(&p, true);
        if (!token[0] || *token == '}') {
            break;
        }
        if (*token == '{') {
            continue;
        }
        if (Q_stricmp(token, "loadmenu") == 0) {
            if (Load_Menu(&p)) {
                continue;
            }
            break;
        }
        Q_DPrintln("Unknown keyword '{}' in menus file {}", token, listFile);
    }

    COM_EndParseSession();
}

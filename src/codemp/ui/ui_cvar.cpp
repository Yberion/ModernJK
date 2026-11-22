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

#include "ui_local.h"

//
// Cvar callbacks
//

static std::optional<screenshotFormat_t> UI_GetScreenshotFormatForString(const char* str)
{
    if (!Q_stricmp(str, "jpg") || !Q_stricmp(str, "jpeg"))
        return screenshotFormat_t::SSF_JPEG;
    else if (!Q_stricmp(str, "tga"))
        return screenshotFormat_t::SSF_TGA;
    else if (!Q_stricmp(str, "png"))
        return screenshotFormat_t::SSF_PNG;
    else
        return std::nullopt;
}

static const char* UI_GetScreenshotFormatString(screenshotFormat_t format)
{
    switch (format) {
    default:
    case screenshotFormat_t::SSF_JPEG:
        return "jpg";
    case screenshotFormat_t::SSF_TGA:
        return "tga";
    case screenshotFormat_t::SSF_PNG:
        return "png";
    }
}

static void UI_UpdateScreenshot(void)
{
    bool changed = false;
    // check some things
    if (ui_screenshotType.string[0] && isalpha(ui_screenshotType.string[0])) {
        const std::optional<screenshotFormat_t> ssf = UI_GetScreenshotFormatForString(ui_screenshotType.string);
        if (!ssf) {
            Q_Println("UI Screenshot Format Type '{}' unrecognised, defaulting to JPEG", ui_screenshotType.string);
            uiInfo.uiDC.screenshotFormat = screenshotFormat_t::SSF_JPEG;
            changed = true;
        }
        else
            uiInfo.uiDC.screenshotFormat = *ssf;
    }
    else if (ui_screenshotType.integer < to_underlying(screenshotFormat_t::SSF_JPEG) ||
             ui_screenshotType.integer > to_underlying(screenshotFormat_t::SSF_PNG)) {
        Q_Println("ui_screenshotType {} is out of range, defaulting to 0 (JPEG)", ui_screenshotType.integer);
        uiInfo.uiDC.screenshotFormat = screenshotFormat_t::SSF_JPEG;
        changed = true;
    }
    else {
        uiInfo.uiDC.screenshotFormat = static_cast<screenshotFormat_t>(atoi(ui_screenshotType.string));
        changed = true;
    }

    if (changed) {
        trap->Cvar_Set("ui_screenshotType", UI_GetScreenshotFormatString(uiInfo.uiDC.screenshotFormat));
        trap->Cvar_Update(&ui_screenshotType);
    }
}

//
// Cvar table
//

typedef struct cvarTable_s {
    vmCvar_t* vmCvar;
    const char* cvarName;
    const char* defaultString;
    void (*update)(void);
    cvarFlags_t cvarFlags;
} cvarTable_t;

#define XCVAR_DECL
#include "ui_xcvar.h"
#undef XCVAR_DECL

static const cvarTable_t uiCvarTable[] = {
#define XCVAR_LIST
#include "ui_xcvar.h"
#undef XCVAR_LIST
};
static const size_t uiCvarTableSize = ARRAY_LEN(uiCvarTable);

void UI_RegisterCvars(void)
{
    size_t i = 0;
    const cvarTable_t* cv = nullptr;

    for (i = 0, cv = uiCvarTable; i < uiCvarTableSize; i++, cv++) {
        trap->Cvar_Register(cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags);
        if (cv->update)
            cv->update();
    }
}

void UI_UpdateCvars(void)
{
    size_t i = 0;
    const cvarTable_t* cv = nullptr;

    for (i = 0, cv = uiCvarTable; i < uiCvarTableSize; i++, cv++) {
        if (cv->vmCvar) {
            int modCount = cv->vmCvar->modificationCount;
            trap->Cvar_Update(cv->vmCvar);
            if (cv->vmCvar->modificationCount != modCount) {
                if (cv->update)
                    cv->update();
            }
        }
    }
}

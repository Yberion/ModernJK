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

#include "cg_local.h"
#include "cg_media.h"
#include <game/objectives.h>
#include "strip_objectives.h"

static const int missionYpos = 79;

const char* showLoadPowersName[] = {
    "INGAME_HEAL2", "INGAME_JUMP2",      "INGAME_SPEED2",       "INGAME_PUSH2",          "INGAME_PULL2",          "INGAME_MINDTRICK2",
    "INGAME_GRIP2", "INGAME_LIGHTNING2", "INGAME_SABER_THROW2", "INGAME_SABER_OFFENSE2", "INGAME_SABER_DEFENSE2", nullptr,
};

// Hack to see if the graphics objectives have been printed.
int obj_graphics[4];

/*
====================
MissionPrint_Line
====================
*/
static void MissionPrint_Line(const ct_table_t color, const objectiveNumber_t objectIndex, int& missionYcnt)
{
    char *str, *strBegin;
    int y, pixelLen, charLen;
    char holdText[1024];
    char finalText[2048];
    qhandle_t graphic;

    int iYPixelsPerLine = cgi_R_Font_HeightPixels(cgs.media.qhFontMedium, 1.0f) * (cgi_Language_IsAsian() ? 1.2f : 1.0f);

    cgi_SP_GetStringText(PACKAGE_OBJECTIVES << 8 | std::to_underlying(objectIndex), finalText, sizeof(finalText));

    pixelLen = cgi_R_Font_StrLenPixels(finalText, cgs.media.qhFontMedium, 1.0f);

    str = finalText;

    /*	CG_DisplayBoxedText(70,50,500,300,finalText,
                            cgs.media.qhFontSmall,
                            1.0f,
                            colorTable[color]
                            );
    */
    if (pixelLen < 500) // One shot - small enough to print entirely on one line
    {
        y = missionYpos + (iYPixelsPerLine * (missionYcnt));
        if (obj_graphics[0]) {
            y += 32 + 4;
        }
        if (obj_graphics[1]) {
            y += 32 + 4;
        }
        if (obj_graphics[2]) {
            y += 32 + 4;
        }
        // CG_DrawProportionalString(108, y,str, textStyleFlags_t::CG_SMALLFONT, colorTable[color] );
        cgi_R_Font_DrawString(108, y, str, colorTable[color], cgs.media.qhFontMedium, -1, 1.0f);
        ++missionYcnt;
    }
    // Text is too long, break into lines.
    else {
        char holdText2[2];
        pixelLen = 0;
        charLen = 0;
        holdText2[1] = '\0';
        strBegin = str;

        while (*str) {
            holdText2[0] = *str;
            pixelLen += cgi_R_Font_StrLenPixels(holdText2, cgs.media.qhFontMedium, 1.0f);

            pixelLen += 2; // For kerning
            ++charLen;

            if (pixelLen > 500) { // Reached max length of this line
                // step back until we find a space
                while ((charLen) && (*str != ' ')) {
                    --str;
                    --charLen;
                }

                if (*str == ' ') {
                    ++str; // To get past space
                }

                Q_strncpyz(holdText, strBegin, charLen);
                holdText[charLen] = '\0';
                strBegin = str;
                pixelLen = 0;
                charLen = 1;

                y = missionYpos + (iYPixelsPerLine * missionYcnt);

                CG_DrawProportionalString(108, y, holdText, textStyleFlags_t::CG_SMALLFONT, colorTable[color]);
                ++missionYcnt;
            }
            else if (*(str + 1) == '\0') {
                ++charLen;

                y = missionYpos + (iYPixelsPerLine * missionYcnt);

                Q_strncpyz(holdText, strBegin, charLen);
                CG_DrawProportionalString(108, y, holdText, textStyleFlags_t::CG_SMALLFONT, colorTable[color]);
                ++missionYcnt;
                break;
            }
            ++str;
        }
    }

    // Special case hack
    if (objectIndex == objectiveNumber_t::DOOM_COMM_OBJ4) {
        y = missionYpos + (iYPixelsPerLine * missionYcnt);
        graphic = cgi_R_RegisterShaderNoMip("textures/system/securitycode");
        CG_DrawPic(320 - (128 / 2), y + 8, 128, 32, graphic);
        obj_graphics[0] = true;
    }
    else if (objectIndex == objectiveNumber_t::KEJIM_POST_OBJ3) {
        y = missionYpos + (iYPixelsPerLine * missionYcnt);
        graphic = cgi_R_RegisterShaderNoMip("textures/system/securitycode_red");
        CG_DrawPic(320 - (32 / 2), y + 8, 32, 32, graphic);
        obj_graphics[1] = true;
    }
    else if (objectIndex == objectiveNumber_t::KEJIM_POST_OBJ4) {
        y = missionYpos + (iYPixelsPerLine * missionYcnt);
        if (obj_graphics[1]) {
            y += 32 + 4;
        }
        graphic = cgi_R_RegisterShaderNoMip("textures/system/securitycode_green");
        CG_DrawPic(320 - (32 / 2), y + 8, 32, 32, graphic);
        obj_graphics[2] = true;
    }
    else if (objectIndex == objectiveNumber_t::KEJIM_POST_OBJ5) {
        y = missionYpos + (iYPixelsPerLine * missionYcnt);
        if (obj_graphics[1]) {
            y += 32 + 4;
        }
        if (obj_graphics[2]) {
            y += 32 + 4;
        }
        graphic = cgi_R_RegisterShaderNoMip("textures/system/securitycode_blue");
        CG_DrawPic(320 - (32 / 2), y + 8, 32, 32, graphic);
        obj_graphics[3] = true;
    }
}

/*
====================
MissionInformation_Draw
====================
*/
void MissionInformation_Draw(centity_t* cent)
{
    int totalY;
    int iYPixelsPerLine = cgi_R_Font_HeightPixels(cgs.media.qhFontMedium, 1.0f) * (cgi_Language_IsAsian() ? 1.2f : 1.0f);

    missionInfo_Updated = false; // This will stop the text from flashing
    cg.missionInfoFlashTime = 0;

    // Frame
    char text[1024] = {0};
    cgi_SP_GetStringTextString("INGAME_OBJECTIVES", text, sizeof(text));
    cgi_R_Font_DrawString(96, missionYpos - 23, text, colorTable[ct_table_t::CT_WHITE], cgs.media.qhFontMedium, -1, 1.0f);

    int missionYcnt = 0;

    obj_graphics[0] = obj_graphics[1] = obj_graphics[2] = obj_graphics[3] = false;

    // Print active objectives
    cgi_R_SetColor(colorTable[ct_table_t::CT_BLUE3]);
    for (objectiveNumber_t o = objectiveNumber_t::KEJIM_POST_OBJ1; o < objectiveNumber_t::MAX_OBJECTIVES; o++) {
        if (cent->gent->client->sess.mission_objectives[o].display) {
            totalY = missionYpos + (iYPixelsPerLine * (missionYcnt)) + (iYPixelsPerLine / 2);
            if (obj_graphics[0]) {
                totalY += 32 + 4;
            }
            if (obj_graphics[1]) {
                totalY += 32 + 4;
            }
            if (obj_graphics[2]) {
                totalY += 32 + 4;
            }
            if (obj_graphics[3]) {
                totalY += 32 + 4;
            }

            //	OBJECTIVE_STAT_PENDING
            CG_DrawPic(88, totalY, 16, 16, cgs.media.messageObjCircle); // Circle in front
            if (cent->gent->client->sess.mission_objectives[o].status == OBJECTIVE_STAT_SUCCEEDED) {
                CG_DrawPic(88, totalY, 16, 16, cgs.media.messageLitOn); // Center Dot
            }
            MissionPrint_Line(ct_table_t::CT_BLUE3, o, missionYcnt);
        }
    }

    if (!missionYcnt) {
        cgi_SP_GetStringTextString("INGAME_OBJNONE", text, sizeof(text));
        //		CG_DrawProportionalString(108, missionYpos, text, textStyleFlags_t::CG_SMALLFONT, colorTable[ct_table_t::CT_LTBLUE1] );
        cgi_R_Font_DrawString(108, missionYpos, text, colorTable[ct_table_t::CT_LTBLUE1], cgs.media.qhFontMedium, -1, 1.0f);
    }
}

/*

//-------------------------------------------------------
static void CG_DrawForceCount( const forcePowers_t force, int x, float *y, const int pad,bool *hasForcePowers )
{
    char	s[MAX_STRING_CHARS];
    int		val, textColor;
    char	text[1024]={0};

    gi.Cvar_VariableStringBuffer( Q_Format("playerfplvl{}",  force ).c_str(),s, sizeof(s) );

    sscanf( s, "%d",&val );

    if ((val<1) || (val> std::to_underlying( forcePowers_t::NUM_FORCE_POWERS )))
    {
        return;
    }

    textColor = ct_table_t::CT_ICON_BLUE;

    // Draw title
    cgi_SP_GetStringTextString( showLoadPowersName[force], text, sizeof(text) );
    CG_DrawProportionalString( x, *y, text, textStyleFlags_t::CG_BIGFONT, colorTable[textColor] );


    // Draw icons
    cgi_R_SetColor( colorTable[ct_table_t::CT_WHITE]);
    const int iconSize = 30;
    if ( val >= 0 )
    {
        x -= 10;	// Back up from title a little

        for ( int i = 0; i < val; i++ )
        {
            CG_DrawPic( x - iconSize - i * (iconSize + 10) , *y, iconSize, iconSize, force_icons[force] );
        }
    }

    *y += pad;

    *hasForcePowers = true;
}


/*
====================
CG_LoadScreen_PersonalInfo
====================
*/
/*
static void CG_LoadScreen_PersonalInfo(void)
{
    float	x, y;
    int		pad = 25;
    char	text[1024]={0};
    bool	hasForcePowers;

    y = 65 + 30;

    pad = 28;
    x = 300;
    hasForcePowers=false;

    CG_DrawForceCount( forcePowers_t::FP_HEAL, x, &y, pad,&hasForcePowers);
    CG_DrawForceCount( forcePowers_t::FP_LEVITATION, x, &y, pad,&hasForcePowers );
    CG_DrawForceCount( forcePowers_t::FP_SPEED, x, &y, pad,&hasForcePowers );
    CG_DrawForceCount( forcePowers_t::FP_PUSH, x, &y, pad,&hasForcePowers );
    CG_DrawForceCount( forcePowers_t::FP_PULL, x, &y, pad,&hasForcePowers );
    CG_DrawForceCount( forcePowers_t::FP_TELEPATHY, x, &y, pad,&hasForcePowers );
    CG_DrawForceCount( forcePowers_t::FP_GRIP, x, &y, pad,&hasForcePowers );
    CG_DrawForceCount( forcePowers_t::FP_LIGHTNING, x, &y, pad,&hasForcePowers );
    CG_DrawForceCount( forcePowers_t::FP_SABERTHROW, x, &y, pad,&hasForcePowers );
    CG_DrawForceCount( forcePowers_t::FP_SABER_OFFENSE, x, &y, pad,&hasForcePowers );
    CG_DrawForceCount( forcePowers_t::FP_SABER_DEFENSE, x, &y, pad,&hasForcePowers );

    if (hasForcePowers)
    {
        cgi_SP_GetStringTextString( "INGAME_CURRENTFORCEPOWERS", text, sizeof(text) );
        CG_DrawProportionalString( 200, 65, text, textStyleFlags_t::CG_CENTER | textStyleFlags_t::CG_BIGFONT, colorTable[ct_table_t::CT_WHITE] );
    }
    else
    {	//you are only totally empty on the very first map?
//		cgi_SP_GetStringTextString( "INGAME_NONE", text, sizeof(text) );
//		CG_DrawProportionalString( 320, y+30, text, textStyleFlags_t::CG_CENTER | textStyleFlags_t::CG_BIGFONT, colorTable[ct_table_t::CT_ICON_BLUE] );
        cgi_SP_GetStringTextString( "INGAME_ALONGTIME", text, sizeof(text) );
        int w = cgi_R_Font_StrLenPixels(text,cgs.media.qhFontMedium, 1.5f);
        cgi_R_Font_DrawString((320)-(w/2), y+40, text,  colorTable[ct_table_t::CT_ICON_BLUE], cgs.media.qhFontMedium, -1, 1.5f);
    }

}
*/

static void CG_LoadBar(void)
{
    const int numticks = 9, tickwidth = 40, tickheight = 8;
    const int tickpadx = 20, tickpady = 12;
    const int capwidth = 8;
    const int barwidth = numticks * tickwidth + tickpadx * 2 + capwidth * 2, barleft = ((640 - barwidth) / 2);
    const int barheight = tickheight + tickpady * 2, bartop = 480 - barheight;
    const int capleft = barleft + tickpadx, tickleft = capleft + capwidth, ticktop = bartop + tickpady;

    cgi_R_SetColor(colorTable[ct_table_t::CT_WHITE]);
    // Draw background
    CG_DrawPic(barleft, bartop, barwidth, barheight, cgs.media.levelLoad);

    // Draw left cap (backwards)
    CG_DrawPic(tickleft, ticktop, -capwidth, tickheight, cgs.media.loadTickCap);

    // Draw bar
    CG_DrawPic(tickleft, ticktop, tickwidth * cg.loadLCARSStage, tickheight, cgs.media.loadTick);

    // Draw right cap
    CG_DrawPic(tickleft + tickwidth * cg.loadLCARSStage, ticktop, capwidth, tickheight, cgs.media.loadTickCap);
}

/*
====================
CG_DrawInformation

Draw all the status / pacifier stuff during level loading
overylays UI_DrawConnectText from ui_connect.cpp
====================
*/
void CG_DrawInformation(void)
{
    int y;

    // draw the dialog background
    const char* info = CG_ConfigString(CS_SERVERINFO);
    const char* s = Info_ValueForKey(info, "mapname");
    qhandle_t levelshot;

    if (!strcmp(s, "bespin_undercity")) // this map has no levelshot
        levelshot = cgi_R_RegisterShaderNoMip("levelshots/kejim_post");
    else
        levelshot = cgi_R_RegisterShaderNoMip(Q_Format("levelshots/{}", s).c_str());

    if (!levelshot) {
        levelshot = cgi_R_RegisterShaderNoMip("menu/art/unknownmap");
    }

    extern SavedGameJustLoaded_e g_eSavedGameJustLoaded; // hack! (hey, it's the last week of coding, ok?
#ifdef JK2_MODE
    if (!levelshot || g_eSavedGameJustLoaded == SavedGameJustLoaded_e::eFULL) {
        // keep whatever's in the screen buffer so far (either the last ingame rendered-image (eg for maptransition)
        //	or the screenshot built-in to a loaded save game...
        //
        cgi_R_DrawScreenShot(0, 0, 640, 480);
    }
    else
#endif
    {
        // put up the pre-defined levelshot for this map...
        //
        cgi_R_SetColor(nullptr);
        CG_DrawPic(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, levelshot);
    }

    if (g_eSavedGameJustLoaded != SavedGameJustLoaded_e::eFULL && !strcmp(s, "kejim_post")) // special case for first map!
    {
        char text[1024] = {0};
        cgi_SP_GetStringTextString("INGAME_ALONGTIME", text, sizeof(text));

        int w = cgi_R_Font_StrLenPixels(text, cgs.media.qhFontMedium, 1.0f);
        cgi_R_Font_DrawString((320) - (w / 2), 140, text, colorTable[ct_table_t::CT_ICON_BLUE], cgs.media.qhFontMedium, -1, 1.0f);
    }
    else if (cg_missionstatusscreen.integer) {
        CG_MissionCompletion();
    }
    CG_LoadBar();

    // the first 150 rows are reserved for the client connection
    // screen to write into
    if (cg.processedSnapshotNum == 0) {
        // still loading
        // print the current item being loaded

#ifndef NDEBUG
        cgi_R_Font_DrawString(48, 398, Q_Format("LOADING ... {}", cg.infoScreenText).c_str(), colorTable[ct_table_t::CT_LTGOLD1], cgs.media.qhFontSmall, -1,
                              1.0f);
#endif
    }

    // draw info string information

    y = 20;
    // map-specific message (long map name)
    s = CG_ConfigString(CS_MESSAGE);

    if (s[0]) {
        if (s[0] == '@') {
            char text[1024] = {0};
            cgi_SP_GetStringTextString(s + 1, text, sizeof(text));
            cgi_R_Font_DrawString(15, y, Q_Format("\"{}\"", text).c_str(), colorTable[ct_table_t::CT_WHITE], cgs.media.qhFontMedium, -1, 1.0f);
        }
        else {
            cgi_R_Font_DrawString(15, y, Q_Format("\"{}\"", s).c_str(), colorTable[ct_table_t::CT_WHITE], cgs.media.qhFontMedium, -1, 1.0f);
        }
        y += 20;
    }
}

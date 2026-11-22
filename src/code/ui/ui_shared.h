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

#ifndef __UI_SHARED_H
#define __UI_SHARED_H

enum class screenshotFormat_t { SSF_JPEG = 0, SSF_TGA, SSF_PNG };

inline constexpr int MAX_OPEN_MENUS = 16;
inline constexpr int MAX_TEXTSCROLL_LINES = 256;

inline constexpr int MAX_EDITFIELD = 256;

inline constexpr double SLIDER_WIDTH = 128.0;
inline constexpr double SLIDER_HEIGHT = 16.0;
inline constexpr double SLIDER_THUMB_WIDTH = 12.0;
inline constexpr double SLIDER_THUMB_HEIGHT = 16.0;
inline constexpr double SCROLLBAR_SIZE = 16.0;

// FIXME: combine flags into bitfields to save space
// FIXME: consolidate all of the common stuff in one structure for menus and items
// THINKABOUTME: is there any compelling reason not to have items contain items
// and do away with a menu per say.. major issue is not being able to dynamically allocate
// and destroy stuff.. Another point to consider is adding an alloc free call for vm's and have
// the engine just allocate the pool for it based on a cvar
// many of the vars are re-used for different item types, as such they are not always named appropriately
// the benefits of c++ in DOOM will greatly help crap like this
// FIXME: need to put a type ptr that points to specific type info per type
//
inline constexpr int MAX_LB_COLUMNS = 16;

typedef struct columnInfo_s {
    int pos;
    int width;
    int maxChars;
} columnInfo_t;

typedef struct listBoxDef_s {
    int startPos;
    int endPos;
    int drawPadding;
    int cursorPos;
    float elementWidth;
    float elementHeight;
    int elementStyle;
    int numColumns;
    columnInfo_t columnInfo[MAX_LB_COLUMNS];
    const char* doubleClick;
    bool notselectable;
    // JLF MPMOVED
    bool scrollhidden;
} listBoxDef_t;

typedef struct editFieldDef_s {
    float minVal;      //	edit field limits
    float maxVal;      //
    float defVal;      //
    float range;       //
    int maxChars;      // for edit fields
    int maxPaintChars; // for edit fields
    int paintOffset;   //
} editFieldDef_t;

inline constexpr int MAX_MULTI_CVARS = 64; // 32

typedef struct multiDef_s {
    const char* cvarList[MAX_MULTI_CVARS];
    const char* cvarStr[MAX_MULTI_CVARS];
    float cvarValue[MAX_MULTI_CVARS];
    int count;
    bool strDef;
} multiDef_t;

enum class itemCvarFlags_t {
    CVAR_NOACTION = 0, // no cvar test (CVAR_NONE is the engine's cvar flag)
    CVAR_ENABLE = 0x00000001,
    CVAR_DISABLE = 0x00000002,
    CVAR_SHOW = 0x00000004,
    CVAR_HIDE = 0x00000008,
    CVAR_SUBSTRING = 0x00000010, // when using enable or disable, just check for strstr instead of ==
};
template <> inline constexpr bool enableEnumFlags<itemCvarFlags_t> = true;

inline constexpr int STRING_POOL_SIZE = 2 * 1024 * 1024;

inline constexpr int NUM_CROSSHAIRS = 9;

typedef struct {
    qhandle_t qhMediumFont;
    qhandle_t cursor;
    qhandle_t gradientBar;
    qhandle_t scrollBarArrowUp;
    qhandle_t scrollBarArrowDown;
    qhandle_t scrollBarArrowLeft;
    qhandle_t scrollBarArrowRight;
    qhandle_t scrollBar;
    qhandle_t scrollBarThumb;
    qhandle_t buttonMiddle;
    qhandle_t buttonInside;
    qhandle_t solidBox;
    qhandle_t sliderBar;
    qhandle_t sliderThumb;
    sfxHandle_t menuEnterSound;
    sfxHandle_t menuExitSound;
    sfxHandle_t menuBuzzSound;
    sfxHandle_t itemFocusSound;
    sfxHandle_t forceChosenSound;
    sfxHandle_t forceUnchosenSound;
    sfxHandle_t datapadmoveRollSound;
    sfxHandle_t datapadmoveJumpSound;
    sfxHandle_t datapadmoveSaberSound1;
    sfxHandle_t datapadmoveSaberSound2;
    sfxHandle_t datapadmoveSaberSound3;
    sfxHandle_t datapadmoveSaberSound4;
    sfxHandle_t datapadmoveSaberSound5;
    sfxHandle_t datapadmoveSaberSound6;

    sfxHandle_t nullSound;

    float fadeClamp;
    int fadeCycle;
    float fadeAmount;
    float shadowX;
    float shadowY;
    vec4_t shadowColor;
    float shadowFadeClamp;
    bool fontRegistered;

    // player settings
    //	qhandle_t	fxBasePic;
    //	qhandle_t	fxPic[7];
    qhandle_t crosshairShader[NUM_CROSSHAIRS];

} cachedAssets_t;

struct itemDef_s;

typedef struct {

    void (*addRefEntityToScene)(const refEntity_t* re);
    void (*clearScene)();
    void (*drawHandlePic)(float x, float y, float w, float h, qhandle_t asset);
    void (*drawRect)(float x, float y, float w, float h, float size, const vec4_t color);
    void (*drawSides)(float x, float y, float w, float h, float size);
    void (*drawText)(float x, float y, float scale, vec4_t color, const char* text, int iMaxPixelWidth, int style, int iFontIndex);
    void (*drawTextWithCursor)(float x, float y, float scale, vec4_t color, const char* text, int cursorPos, char cursor, int iMaxPixelWidth, int style,
                               int iFontIndex);
    void (*drawTopBottom)(float x, float y, float w, float h, float size);
    void (*executeText)(cbufExec_t exec_when, const char* text);
    int (*feederCount)(float feederID);
    void (*feederSelection)(float feederID, int index, struct itemDef_s* item);
    void (*fillRect)(float x, float y, float w, float h, const vec4_t color);
    void (*getBindingBuf)(fakeAscii_t keynum, char* buf, int buflen);
    void (*getCVarString)(const char* cvar, char* buffer, int bufsize);
    float (*getCVarValue)(const char* cvar);
    bool (*getOverstrikeMode)();
    float (*getValue)(int ownerDraw);
    void (*keynumToStringBuf)(fakeAscii_t keynum, char* buf, int buflen);
    void (*modelBounds)(qhandle_t model, vec3_t min, vec3_t max);
    bool (*ownerDrawHandleKey)(int ownerDraw, int flags, float* special, fakeAscii_t key);
    void (*ownerDrawItem)(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special,
                          float scale, vec4_t color, qhandle_t shader, int textStyle, int iFontIndex);
    bool (*ownerDrawVisible)(int flags);
    int (*ownerDrawWidth)(int ownerDraw, float scale);
    void (*Pause)(bool b);
    int (*registerFont)(const char* pFontname);
    qhandle_t (*registerModel)(const char* p);
    qhandle_t (*registerShaderNoMip)(const char* p);
    sfxHandle_t (*registerSound)(const char* name, bool compressed);
    void (*renderScene)(const refdef_t* fd);
    bool (*runScript)(const char** p);
    bool (*deferScript)(const char** p);
    void (*setBinding)(fakeAscii_t keynum, const char* binding);
    void (*setColor)(const vec4_t v);
    void (*setCVar)(const char* cvar, const char* value);
    void (*setOverstrikeMode)(bool b);
    void (*startLocalSound)(sfxHandle_t sfx, soundChannel_t channelNum);
    void (*stopCinematic)(int handle);
    int (*textHeight)(const char* text, float scale, int iFontIndex);
    int (*textWidth)(const char* text, float scale, int iFontIndex);
    qhandle_t (*feederItemImage)(float feederID, int index);
    const char* (*feederItemText)(float feederID, int index, int column, qhandle_t* handle);

    qhandle_t (*registerSkin)(const char* name);

    // rww - ghoul2 stuff. Add whatever you need here, remember to set it in _UI_Init or it will crash when you try to use it.
    bool (*g2_SetSkin)(CGhoul2Info* ghlInfo, qhandle_t customSkin, qhandle_t renderSkin);
    bool (*g2_SetBoneAnim)(CGhoul2Info* ghlInfo, const char* boneName, const int startFrame, const int endFrame, const boneFlags_t flags, const float animSpeed,
                           const int currentTime, const float setFrame, const int blendTime);
    bool (*g2_RemoveGhoul2Model)(CGhoul2Info_v& ghlInfo, const int modelIndex);
    int (*g2_InitGhoul2Model)(CGhoul2Info_v& ghoul2, const char* fileName, int, qhandle_t customSkin, qhandle_t customShader, int modelFlags, int lodBias);
    void (*g2_CleanGhoul2Models)(CGhoul2Info_v& ghoul2);
    int (*g2_AddBolt)(CGhoul2Info* ghlInfo, const char* boneName);
    bool (*g2_GetBoltMatrix)(CGhoul2Info_v& ghoul2, const int modelIndex, const int boltIndex, mdxaBone_t* matrix, const vec3_t angles, const vec3_t position,
                             const int frameNum, qhandle_t* modelList, const vec3_t scale);
    void (*g2_GiveMeVectorFromMatrix)(mdxaBone_t& boltMatrix, Eorientations flags, vec3_t& vec);

    // Utility functions that don't immediately redirect to ghoul2 functions
    int (*g2hilev_SetAnim)(CGhoul2Info* ghlInfo, const char* boneName, animNumber_t animNum, const bool freeze);

    float yscale;
    float xscale;
    float bias;
    int realTime;
    int frameTime;
    bool cursorShow;
    int cursorx;
    int cursory;
    bool debug;

    cachedAssets_t Assets;

    glconfig_t glconfig;
    qhandle_t whiteShader;
    qhandle_t gradientImage;
    float FPS;

    screenshotFormat_t screenshotFormat;

} displayContextDef_t;

void UI_InitMemory(void);

inline constexpr int MAX_COLOR_RANGES = 10;
inline constexpr int MAX_MENUITEMS = 150;
inline constexpr int MAX_MENUS = 64;

enum class windowFlags_t {
    WINDOW_NONE = 0,
    WINDOW_MOUSEOVER = 0x00000001,         // mouse is over it, non exclusive
    WINDOW_HASFOCUS = 0x00000002,          // has cursor focus, exclusive
    WINDOW_VISIBLE = 0x00000004,           // is visible
    WINDOW_INACTIVE = 0x00000008,          // is visible but grey ( non-active )
    WINDOW_DECORATION = 0x00000010,        // for decoration only, no mouse, keyboard, etc..
    WINDOW_FADINGOUT = 0x00000020,         // fading out, non-active
    WINDOW_FADINGIN = 0x00000040,          // fading in
    WINDOW_MOUSEOVERTEXT = 0x00000080,     // mouse is over it, non exclusive
    WINDOW_INTRANSITION = 0x00000100,      // window is in transition
    WINDOW_FORECOLORSET = 0x00000200,      // forecolor was explicitly set ( used to color alpha images or not )
    WINDOW_HORIZONTAL = 0x00000400,        // for list boxes and sliders, vertical is default this is set of horizontal
    WINDOW_LB_LEFTARROW = 0x00000800,      // mouse is over left/up arrow
    WINDOW_LB_RIGHTARROW = 0x00001000,     // mouse is over right/down arrow
    WINDOW_LB_THUMB = 0x00002000,          // mouse is over thumb
    WINDOW_LB_PGUP = 0x00004000,           // mouse is over page up
    WINDOW_LB_PGDN = 0x00008000,           // mouse is over page down
    WINDOW_ORBITING = 0x00010000,          // item is in orbit
    WINDOW_OOB_CLICK = 0x00020000,         // close on out of bounds click
    WINDOW_WRAPPED = 0x00040000,           // manually wrap text
    WINDOW_AUTOWRAPPED = 0x00080000,       // auto wrap text
    WINDOW_FORCED = 0x00100000,            // forced open
    WINDOW_POPUP = 0x00200000,             // popup
    WINDOW_BACKCOLORSET = 0x00400000,      // backcolor was explicitly set
    WINDOW_TIMEDVISIBLE = 0x00800000,      // visibility timing ( NOT implemented )
    WINDOW_PLAYERCOLOR = 0x01000000,       // hack the forecolor to match ui_char_color_*
    WINDOW_SCRIPTWAITING = 0x02000000,     // delayed script waiting to run
                                           // JLF MPMOVED
    WINDOW_INTRANSITIONMODEL = 0x04000000, // delayed script waiting to run
    WINDOW_IGNORE_ESCAPE = 0x08000000,     // ignore normal closeall menus escape functionality
};
template <> inline constexpr bool enableEnumFlags<windowFlags_t> = true;

typedef struct {
    float x; // horiz position
    float y; // vert position
    float w; // width
    float h; // height;
} rectDef_t;

typedef rectDef_t UIRectangle;

// FIXME: do something to separate text vs window stuff
typedef struct {
    UIRectangle rect;          // client coord rectangle
    UIRectangle rectClient;    // screen coord rectangle
    char* name;                //
    char* group;               // if it belongs to a group
    const char* cinematicName; // cinematic name
    int cinematic;             // cinematic handle
    int style;                 //
    int border;                //
    int ownerDraw;             // ownerDraw style
    int ownerDrawFlags;        // show flags for ownerdraw items
    float borderSize;          //
    windowFlags_t flags;       // visible, focus, mouseover, cursor
    UIRectangle rectEffects;   // for various effects
    UIRectangle rectEffects2;  // for various effects
    int offsetTime;            // time based value for various effects
    int nextTime;              // time next effect should cycle
    int delayTime;             // time when delay expires
    char* delayedScript;       // points into another script's text while delaying
    vec4_t foreColor;          // text color
    vec4_t backColor;          // border color
    vec4_t borderColor;        // border color
    vec4_t outlineColor;       // border color
    qhandle_t background;      // background asset
} windowDef_t;

typedef windowDef_t Window;

typedef struct {
    vec4_t color; //
    float low;    //
    float high;   //
} colorRangeDef_t;

typedef struct modelDef_s {
    int angle;
    vec3_t origin;
    float fov_x;
    float fov_y;
    int rotationSpeed;

    vec3_t g2mins;       // required
    vec3_t g2maxs;       // required
    int g2skin;          // optional
    animNumber_t g2anim; // optional
    // JLF MPMOVED
    // Transition extras
    vec3_t g2mins2, g2maxs2, g2minsEffect, g2maxsEffect;
    float fov_x2, fov_y2, fov_Effectx, fov_Effecty;
} modelDef_t;

#include <shared/qcommon/q_clientflags.h>

typedef struct itemDef_s {
    Window window;              // common positional, border, style, layout info
    UIRectangle textRect;       // rectangle the text ( if any ) consumes
    int type;                   // text, button, radiobutton, checkbox, textfield, listbox, combo
    int alignment;              // left center right
    int textalignment;          // ( optional ) alignment for text within rect based on text width
    float textalignx;           // ( optional ) text alignment x coord
    float textaligny;           // ( optional ) text alignment y coord
    float text2alignx;          // ( optional ) text2 alignment x coord
    float text2aligny;          // ( optional ) text2 alignment y coord
    float textscale;            // scale percentage from 72pts
    int textStyle;              // ( optional ) style, normal and shadowed are it for now
    char* text;                 // display text
    char* text2;                // display text2
    const char* descText;       //	Description text
    void* parent;               // menu owner
    qhandle_t asset;            // handle to asset
    CGhoul2Info_v ghoul2;       // ghoul2 instance if available instead of a model.
    itemModelFlags_t flags;     // flags like g2valid, character, saber, saber2, etc.
    const char* mouseEnterText; // mouse enter script
    const char* mouseExitText;  // mouse exit script
    const char* mouseEnter;     // mouse enter script
    const char* mouseExit;      // mouse exit script
    const char* action;         // select script
                                // JLFACCEPT MPMOVED
    const char* accept;
    // JLFDPADSCRIPT MPMOVED
    const char* selectionNext;
    const char* selectionPrev;

    const char* onFocus;       // select script
    const char* leaveFocus;    // select script
    const char* cvar;          // associated cvar
    const char* cvarTest;      // associated cvar for enable actions
    const char* enableCvar;    // enable, disable, show, or hide based on value, this can contain a list
    itemCvarFlags_t cvarFlags; //	what type of action to take on cvarenables
    sfxHandle_t focusSound;    //
    int numColors;             // number of color ranges
    colorRangeDef_t colorRanges[MAX_COLOR_RANGES];
    float special;      // used for feeder id's etc.. diff per type
    int cursorPos;      // cursor position in characters
    void* typeData;     // type specific data ptr's
    int appearanceSlot; // order of appearance
    int value;          // used by ITEM_TYPE_MULTI that aren't linked to a particular cvar.
    int font;           // FONT_SMALL,FONT_MEDIUM,FONT_LARGE
    int invertYesNo;
    int xoffset;
    bool disabled;       // Does this item ignore mouse and keyboard focus
    bool disabledHidden; // hide the item when 'disabled' is true (for generic image items)
} itemDef_t;

typedef struct {
    Window window;
    const char* font;     // font
    bool fullScreen;      // covers entire screen
    int itemCount;        // number of items;
    int fontIndex;        //
    int cursorItem;       // which item as the cursor
    int fadeCycle;        //
    float fadeClamp;      //
    float fadeAmount;     //
    const char* onOpen;   // run when the menu is first opened
    const char* onClose;  // run when the menu is closed
                          // JLFACCEPT MPMOVED
    const char* onAccept; // run when menu is closed with acceptance

    const char* onESC;     // run when the menu is closed
    const char* soundName; // background loop sound for menu

    vec4_t focusColor;               // focus color for items
    vec4_t disableColor;             // focus color for items
    itemDef_t* items[MAX_MENUITEMS]; // items this menu contains
    float appearanceTime;            //	when next item should appear
    int appearanceCnt;               //	current item displayed
    float appearanceIncrement;       //
    int descX;                       // X position of description
    int descY;                       // X position of description
    vec4_t descColor;                // description text color for items
    int descAlignment;               // Description of alignment
    float descScale;                 // Description scale
    int descTextStyle;               // ( optional ) style, normal and shadowed are it for now

} menuDef_t;

typedef struct textScrollDef_s {
    int startPos;
    int endPos;

    float lineHeight;
    int maxLineChars;
    int drawPadding;

    // changed spelling to make them fall out during compile while I made them asian-aware	-Ste
    //
    int iLineCount;
    const char* pLines[MAX_TEXTSCROLL_LINES]; // can contain nullptr ptrs that you should skip over during paint.

} textScrollDef_t;

typedef struct {
    const char* name;
    bool (*handler)(itemDef_t* item, const char** args);

} commandDef_t;

menuDef_t* Menu_GetFocused(void);

void Controls_GetConfig(void);
void Controls_SetConfig(void);
void Controls_SetDefaults(void);
bool Display_KeyBindPending(void);
bool Display_MouseMove(void* p, int x, int y);
int Display_VisibleMenuCount(void);
bool Int_Parse(const char** p, int* i);
void Init_Display(displayContextDef_t* dc);
void Menus_Activate(menuDef_t* menu);
menuDef_t* Menus_ActivateByName(const char* p);
bool Menus_AnyFullScreenVisible(void);
void Menus_CloseAll(void);
int Menu_Count(void);
itemDef_t* Menu_FindItemByName(menuDef_t* menu, const char* p);
void Menu_ShowGroup(menuDef_t* menu, const char* itemName, bool showFlag);
void Menu_ItemDisable(menuDef_t* menu, const char* name, bool disableFlag);
int Menu_ItemsMatchingGroup(menuDef_t* menu, const char* name);
itemDef_t* Menu_GetMatchingItemByNumber(menuDef_t* menu, int index, const char* name);

void Menu_HandleKey(menuDef_t* menu, int key, bool down);
void Menu_New(char* buffer);
void Menus_OpenByName(const char* p);
void Menu_PaintAll(void);
void Menu_Reset(void);
void PC_EndParseSession(char* buffer);
bool PC_Float_Parse(int handle, float* f);
bool PC_ParseString(const char** tempStr);
bool PC_ParseStringMem(const char** out);
void PC_ParseWarning(const char* message);
bool PC_String_Parse(int handle, const char** out);
int PC_StartParseSession(const char* fileName, char** buffer);
char* PC_ParseExt(void);
bool PC_ParseInt(int* number);
bool PC_ParseFloat(float* number);
bool PC_ParseColor(vec4_t* c);
const char* String_Alloc(const char* p);
void String_Init(void);
bool String_Parse(const char** p, const char** out);
void String_Report(void);
void UI_Cursor_Show(bool flag);
itemDef_t* Menu_GetMatchingItemByNumber(menuDef_t* menu, int index, const char* name);

extern displayContextDef_t* DC;

#endif

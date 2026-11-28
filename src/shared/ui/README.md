# The menus' shared code

The menus of the three games (the multiplayer's UI module and its cgame's HUD menus, Jedi Academy's and Jedi Outcast's single player, whose UI is compiled into the engine) read the same menu files' language and draw the same kinds of items. Their code is being made one: for now the types, `ui_shared.h`, and the reader of the menus' files, `ui_parse.cpp`; each tree still compiles its own `ui/ui_shared.cpp` against them.

## Files

| file | what it does |
|---|---|
| `ui_shared.h` | a menu and its items, their window and its flags, each kind of item's parsed data (a list box, an edit field or a slider, a row of choices, a model, a scrolled text), the cached assets, the display context through which the menus draw and reach their module, and the functions of each tree's `ui_shared.cpp` |
| `ui_parse.cpp` | the menus' reader, compiled by each menus' module (the multiplayer's UI and cgame, the single player's engine) against its own `<ui/ui_shared.h>`: a menu file read whole and split by `COM_ParseExt`, the numbers, texts, colours, rectangles and scripts its keywords take, the names of the game's `menudef.h` read where a number is, a menu file's top level (`UI_ParseMenu`) and a list of menu files (`UI_ParseMenuList`) |

## Rules

- Each tree's `ui/ui_shared.h` gives its game's values, then includes this header: the sliders' width and thumb height its menus are drawn for (`SLIDER_WIDTH`, `SLIDER_THUMB_HEIGHT`), the type of an item's Ghoul2 instance (`uiGhoul2_t`: the multiplayer's handle, the single player's `CGhoul2Info_v`), the display context's functions of that game alone (`displayContextGame_t`, from which the display context derives: the single player's Ghoul2 functions), the file of its menus' names (`MENUDEF_FILE`: Jedi Academy's `ui/jamp/menudef.h`, Jedi Outcast's `ui/jk2mp/menudef.h`).
- Each tree's `ui/ui_shared.cpp` reads a file whole (`UI_ReadMenuFile`: the single player's `FS_ReadFile`, the multiplayer's `FS_Open` and `FS_Read` for the UI and the cgame); each module reads a menu file's `assetGlobalDef` block (`Asset_Parse`) and its menus' keywords (`Menu_New`).
- The reader's functions return true when they read a value, false at the file's end; a number may be on the lines after its keyword. A name of `menudef.h`, from its `#define NAME value` lines whose value is one decimal or hexadecimal number, is read where a number is; a word it does not define is read as 0, with a warning that gives the file and the line. Names are never read in a text or a script.
- A script keeps its tokens as they are written, a token of more than one character quoted; a `;` against a word without quotes is a command's end of its own. An empty quoted string is a value; quoted strings side by side are values of their own.
- A word outside a menu in a menu file is reported with `developer` 1 only; a keyword unknown or not read inside a menu is a warning.
- Until the code is one, a few members keep each game's meaning apart: an item's font is the menu's font in the multiplayer (`iMenuFont`) and the registered font's handle in the single player (`font`); the single player's text functions of the display context, which take a handle and a width in pixels, have names of their own (`drawTextWidth`, `drawTextWithCursorWidth`, `textWidthHandle`, `textHeightHandle`, `ownerDrawItemHandle`).
- A change of these types or of the menus' code is checked by the menus' dump, the same before and after in the three games: `docs/modernization/tools/menudump/` (`menudump_tmpdbg.py` inserts a temporary trace, `dump_sessions.sh` runs the three games).

## Example

```cpp
// a slider's thumb, at its game's width
const float x = item->window.rect.x + (value * SLIDER_WIDTH) - (SLIDER_THUMB_WIDTH / 2);
DC->drawHandlePic(x, item->window.rect.y - 2, SLIDER_THUMB_WIDTH, SLIDER_THUMB_HEIGHT, DC->Assets.sliderThumb);

// a keyword's value: a number, or a name of menudef.h such as ITEM_TYPE_LISTBOX
if (!PC_ParseInt(&item->type)) {
    return false;
}
```

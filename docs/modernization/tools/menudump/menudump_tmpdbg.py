"""A temporary trace (TMPDBG) of every menu and item the menus' code has read, printed to the log after each menu file is
loaded: the multiplayer's UI and cgame, the single player's UI. For comparing two builds (sub-step 3 of plan step 15's
end: nothing may change in parts 1 to 3). Remove it with --remove, then check `grep -rn TMPDBG src`.

Usage: python -I menudump_tmpdbg.py <tree root> [--sp-old] [--remove]
--sp-old: the single player's items hold their type data as a void* (the code before part 1).
"""
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
remove = '--remove' in sys.argv
sp_old = '--sp-old' in sys.argv

BEGIN = '// TMPDBG BEGIN menu dump\n'
END = '// TMPDBG END menu dump\n'


def dump_code(sp):
    td = 'i->typeData' if (sp and sp_old) else 'i->typeData.data'
    font = 'i->font' if sp else 'i->iMenuFont'
    edit_types = ('ITEM_TYPE_EDITFIELD', 'ITEM_TYPE_NUMERICFIELD', 'ITEM_TYPE_YESNO', 'ITEM_TYPE_BIND', 'ITEM_TYPE_SLIDER') + (() if sp else ('ITEM_TYPE_TEXT',))
    edit_test = ' || '.join(f'i->type == {t}' for t in edit_types)
    sp_win_fmt = ' delay={} dscript={}' if sp else ''
    sp_win_args = ', w.delayTime, S(w.delayedScript)' if sp else ''
    sp_item = ', i->value' if sp else ''
    sp_item_fmt = ' value={}' if sp else ''
    sp_menu = ', m->descTextStyle' if sp else ''
    sp_menu_fmt = ' descStyle={}' if sp else ''
    return BEGIN + f'''static const char* S(const char* p)
{{
    return p ? p : "(null)";
}}
static void DumpWindow_TMPDBG(const char* what, const windowDef_t& w)
{{
    Q_Println("TMPDBG {{}} win name={{}} group={{}} rect=({{}} {{}} {{}} {{}}) client=({{}} {{}} {{}} {{}}) flags={{}} style={{}} border={{}} bsize={{}} od={{}} odf={{}} cin={{}} bg={{}} fore=({{}} {{}} {{}} {{}}) back=({{}} {{}} {{}} {{}}) bc=({{}} {{}} {{}} {{}}) oc=({{}} {{}} {{}} {{}}){sp_win_fmt}", what, S(w.name), S(w.group), w.rect.x, w.rect.y,
              w.rect.w, w.rect.h, w.rectClient.x, w.rectClient.y, w.rectClient.w, w.rectClient.h, w.flags, w.style, w.border, w.borderSize, w.ownerDraw, w.ownerDrawFlags, S(w.cinematicName),
              w.background, w.foreColor[0], w.foreColor[1], w.foreColor[2], w.foreColor[3], w.backColor[0], w.backColor[1], w.backColor[2], w.backColor[3], w.borderColor[0],
              w.borderColor[1], w.borderColor[2], w.borderColor[3], w.outlineColor[0], w.outlineColor[1], w.outlineColor[2], w.outlineColor[3]{sp_win_args});
}}
void UI_DumpMenus_TMPDBG(const char* where)
{{
    Q_Println("TMPDBG dump after {{}}: {{}} menus", where, menuCount);
    for (int mi = 0; mi < menuCount; mi++) {{
        const menuDef_t* m = &Menus[mi];
        DumpWindow_TMPDBG("menu", m->window);
        Q_Println("TMPDBG menu font={{}} full={{}} items={{}} fontIndex={{}} cursor={{}} fade=({{}} {{}} {{}}) open={{}} close={{}} accept={{}} esc={{}} sound={{}} focus=({{}} {{}} {{}} {{}}) disable=({{}} {{}} {{}} {{}}) desc=({{}} {{}} {{}} {{}}) dc=({{}} {{}} {{}} {{}}) appear=({{}} {{}} {{}}){sp_menu_fmt}", S(m->font),
                  m->fullScreen, m->itemCount, m->fontIndex, m->cursorItem, m->fadeCycle, m->fadeClamp, m->fadeAmount, S(m->onOpen), S(m->onClose), S(m->onAccept), S(m->onESC),
                  S(m->soundName), m->focusColor[0], m->focusColor[1], m->focusColor[2], m->focusColor[3], m->disableColor[0], m->disableColor[1], m->disableColor[2],
                  m->disableColor[3], m->descX, m->descY, m->descAlignment, m->descScale, m->descColor[0], m->descColor[1], m->descColor[2], m->descColor[3],
                  m->appearanceTime, m->appearanceCnt, m->appearanceIncrement{sp_menu});
        for (int ii = 0; ii < m->itemCount; ii++) {{
            const itemDef_t* i = m->items[ii];
            DumpWindow_TMPDBG("item", i->window);
            Q_Println("TMPDBG item type={{}} align={{}} talign={{}} tx={{}} ty={{}} tscale={{}} tstyle={{}} text={{}} text2={{}} t2=({{}} {{}}) asset={{}} flags={{}} menter={{}} mexit={{}} menters={{}} mexits={{}} action={{}} accept={{}} next={{}} prev={{}} focus={{}} leave={{}} cvar={{}} ctest={{}} ecvar={{}} cflags={{}} fsound={{}} colors={{}} special={{}} cursor={{}} desc={{}} slot={{}} font={{}} dis={{}} inv={{}} xoff={{}} dishid={{}}{sp_item_fmt}",
                      i->type, i->alignment, i->textalignment, i->textalignx, i->textaligny, i->textscale, i->textStyle, S(i->text), S(i->text2), i->text2alignx, i->text2aligny, i->asset,
                      i->flags, S(i->mouseEnterText), S(i->mouseExitText), S(i->mouseEnter), S(i->mouseExit), S(i->action), S(i->accept), S(i->selectionNext), S(i->selectionPrev),
                      S(i->onFocus), S(i->leaveFocus), S(i->cvar), S(i->cvarTest), S(i->enableCvar), i->cvarFlags, i->focusSound, i->numColors, i->special, i->cursorPos, S(i->descText),
                      i->appearanceSlot, {font}, i->disabled, i->invertYesNo, i->xoffset, i->disabledHidden{sp_item});
            for (int c = 0; c < i->numColors; c++) {{
                Q_Println("TMPDBG color {{}} {{}} ({{}} {{}} {{}} {{}})", i->colorRanges[c].low, i->colorRanges[c].high, i->colorRanges[c].color[0], i->colorRanges[c].color[1],
                          i->colorRanges[c].color[2], i->colorRanges[c].color[3]);
            }}
            const void* data = {td};
            if (!data) {{
                continue;
            }}
            if (i->type == ITEM_TYPE_LISTBOX) {{
                const auto* l = static_cast<const listBoxDef_t*>(data);
                Q_Println("TMPDBG listbox start={{}} end={{}} pad={{}} cursor={{}} ew={{}} eh={{}} estyle={{}} cols={{}} dclick={{}} nosel={{}} hidden={{}}", l->startPos, l->endPos, l->drawPadding, l->cursorPos,
                          l->elementWidth, l->elementHeight, l->elementStyle, l->numColumns, S(l->doubleClick), l->notselectable, l->scrollhidden);
                for (int c = 0; c < l->numColumns && c < MAX_LB_COLUMNS; c++) {{
                    Q_Println("TMPDBG column {{}} {{}} {{}}", l->columnInfo[c].pos, l->columnInfo[c].width, l->columnInfo[c].maxChars);
                }}
            }}
            else if ({edit_test}) {{
                const auto* e = static_cast<const editFieldDef_t*>(data);
                Q_Println("TMPDBG edit min={{}} max={{}} def={{}} range={{}} chars={{}} paint={{}} offset={{}}", e->minVal, e->maxVal, e->defVal, e->range, e->maxChars, e->maxPaintChars, e->paintOffset);
            }}
            else if (i->type == ITEM_TYPE_MULTI) {{
                const auto* u = static_cast<const multiDef_t*>(data);
                Q_Println("TMPDBG multi count={{}} str={{}}", u->count, u->strDef);
                for (int c = 0; c < u->count && c < MAX_MULTI_CVARS; c++) {{
                    Q_Println("TMPDBG choice {{}} {{}} {{}}", S(u->cvarList[c]), S(u->cvarStr[c]), u->cvarValue[c]);
                }}
            }}
            else if (i->type == ITEM_TYPE_MODEL) {{
                const auto* o = static_cast<const modelDef_t*>(data);
                Q_Println("TMPDBG model angle={{}} origin=({{}} {{}} {{}}) fov=({{}} {{}}) rot={{}} mins=({{}} {{}} {{}}) maxs=({{}} {{}} {{}}) skin={{}} anim={{}}", o->angle, o->origin[0], o->origin[1], o->origin[2],
                          o->fov_x, o->fov_y, o->rotationSpeed, o->g2mins[0], o->g2mins[1], o->g2mins[2], o->g2maxs[0], o->g2maxs[1], o->g2maxs[2], o->g2skin, o->g2anim);
            }}
            else if (i->type == ITEM_TYPE_TEXTSCROLL) {{
                const auto* t = static_cast<const textScrollDef_t*>(data);
                Q_Println("TMPDBG textscroll start={{}} end={{}} lh={{}} maxc={{}} pad={{}} lines={{}}", t->startPos, t->endPos, t->lineHeight, t->maxLineChars, t->drawPadding, t->iLineCount);
            }}
        }}
    }}
}}
''' + END


def strip(text):
    while BEGIN in text:
        a = text.index(BEGIN)
        b = text.index(END, a) + len(END)
        text = text[:a] + text[b:]
    return text


targets = [
    ('src/codemp/ui/ui_shared.cpp', False),
    ('src/code/ui/ui_shared.cpp', True),
]
calls = [
    # the multiplayer's UI_LoadMenus: before part 2 (botlib's precompiler), then after it
    ('src/codemp/ui/ui_main.cpp', ('    trap->PC_RemoveAllGlobalDefines();\n}\n',
                                   '    //	Q_Println("UI menu load time = {} milli seconds", trap->Milliseconds() - start);\n}\n'),
     '    UI_DumpMenus_TMPDBG(menuFile);\n'),
    ('src/codemp/cgame/cg_main.cpp', '    // Q_Println("UI menu load time = {} milli seconds", cgi_Milliseconds() - start);\n}\n', '    UI_DumpMenus_TMPDBG(menuFile);\n'),
    ('src/code/ui/ui_main.cpp', '    ui.FS_FreeFile(buffer); // let go of the buffer\n}\n', '    UI_DumpMenus_TMPDBG(menuFile);\n'),
]
DECL = 'void UI_DumpMenus_TMPDBG(const char* where); // TMPDBG\n'

for path, sp in targets:
    p = root / path
    text = strip(p.read_text(encoding='utf-8')).rstrip('\n') + '\n'
    if not remove:
        text = text + '\n' + dump_code(sp)
    p.write_text(text, encoding='utf-8', newline='\n')
    print(('removed from ' if remove else 'added to ') + path)

for path, anchor, call in calls:
    p = root / path
    text = p.read_text(encoding='utf-8')
    text = text.replace(call.rstrip('\n') + ' // TMPDBG\n', '').replace(DECL, '')
    if not remove:
        if isinstance(anchor, tuple):
            anchor = next(a for a in anchor if a in text)
        assert text.count(anchor) == 1, (path, anchor)
        text = text.replace(anchor, anchor[:-2] + call.rstrip('\n') + ' // TMPDBG\n}\n')
        # the declaration before the file's first function definition that calls it: at the top, after the includes
        last_include = max(m.end() for m in re.finditer(r'^#include [^\n]*\n', text, flags=re.M))
        text = text[:last_include] + DECL + text[last_include:]
    p.write_text(text, encoding='utf-8', newline='\n')
    print(('removed from ' if remove else 'added to ') + path)

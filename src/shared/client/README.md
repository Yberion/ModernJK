# The clients' shared code

The code the multiplayer's client and the single player's (which builds Jedi Academy and Jedi Outcast) have in common, written once: one static library, `modernjk_client`, which needs no engine, and the engines' own code made one (`engine/`), which each client compiles against its own headers. Each tree keeps a small part of its own, in its `client/` folder.

## In the game

The console's line, the chat's (the multiplayer's) and the binds work the same in the three games. A long line typed in the console keeps its cursor in view: End or Ctrl-E shows the line's last characters, Home or Ctrl-A its first ones, a character typed in the middle moves the cursor and not the text. The right Shift, Ctrl and Alt are keys of their own everywhere (`RSHIFT`, `RCTRL`, `RALT`), so a bind on one of them comes back on it when the game starts again; the single player used to write it under the left key's name.

## Files

| file | what it does |
|---|---|
| `field.h`, `field.cpp` | a line of text being typed (`field_t`, one definition for the trees): what a key (Delete, the arrows, Home, End, Insert) or a character (a printable one, Ctrl-H, Ctrl-C, Ctrl-V, Ctrl-A, Ctrl-E) does to it, and which part of it is drawn; the clipboard and the overstrike switch are asked of the engine by what the functions return |
| `keynames.h`, `keynames.cpp` | the engine's table of the keys (`keyname_t`, `keyNames_t`), the name a bind writes for a key and the key a name gives back, and the sixteen free codes the keys without a code of their own take with their names (`DynamicKeys`) |
| `engine/keys.h`, `engine/cl_keys.cpp` | the keys as a client runs them, compiled by each client: the keys' states and binds, the console's line and its history, the edit fields drawn, the bind commands, a key or a character given to the console or to what the tree names |

Each tree's `client/cl_keys.cpp` writes the functions `engine/keys.h` lists under "each tree's client/cl_keys.cpp": what a key does while a video plays, what Escape closes or opens, and where a key or a character goes beside the console (the multiplayer's download question, UI module, cgame module and chat; the single player's UI).

## Rules

- `modernjk_client` follows the C++ Core Guidelines (GUIDELINES section 5), with the strict warnings netcodec has and clang-tidy at zero (`.clang-tidy` here); its types at the engines' boundary keep the engines' shape (`field_t`'s C string, `keyname_t`).
- `engine/` is the engines' own code, made one: it keeps the engines' style, and is not run through clang-tidy.
- A function of the shared files exists once, with one behaviour in the three games; a tree's part is kept to small functions of the same name in each tree, which call that tree's own modules (GUIDELINES section 6).
- After every key and character, a field's cursor is in view (its column between 0 and the field's last) and the line is scrolled no further than it needs; what a field shows stays in its line whatever the field holds.
- The clipboard is typed into a field one printable character at a time: a control character of the clipboard is left out, since it would act as its key.
- No two keys of the table share a name, and a key's name reads back as the key.

## Example

```cpp
// a character the player typed, into the console's line
if (client_input::TypeCharacter(g_consoleField, character, kg.key_overstrikeMode) == client_input::FieldRequest::Paste) {
    Field_Paste(&g_consoleField);
}

// the right Ctrl's name in a bind, and back
const client_input::KeyText text = client_input::NameOfKey(keynames, fakeAscii_t::A_CTRL2); // "RCTRL"
const std::optional<fakeAscii_t> key = client_input::KeyOfName(keynames, text.c_str());   // fakeAscii_t::A_CTRL2
```

## Tests

`test/client` (`modernjk.client` in `ctest`): characters typed, inserted and put in place, the line's room; the control characters; the keys; the cursor kept in view on a line longer than the field (End and Ctrl-E showing the last characters, a short line ended then typed past the field never scrolled from a negative place); what a field shows whatever it holds; no two keys of the table named alike; every key's name read back as the key; the names' forms (one character, a name in any case, `0x` and two digits); the free codes given, found again, taken back, none left.

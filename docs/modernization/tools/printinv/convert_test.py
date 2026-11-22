"""Checks convert.py on the forms the multiplayer's calls take. Usage: python -I convert_test.py"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import convert  # noqa: E402

STRIP = [
    ('S_COLOR_YELLOW "WARNING: %s\\n"', '"%s\\n"'),
    ('"^3WARNING: foo"', '"foo"'),
    ('S_COLOR_YELLOW "WARNING: " "rest %d"', '"rest %d"'),
    ('"WARNING: blah"', '"blah"'),
    ('S_COLOR_RED "ERROR : x"', '"x"'),
    ('"foo"', '"foo"'),
    ('S_COLOR_YELLOW "foo"', '"foo"'),
    ('buffer', 'buffer'),
]
TARGET = [
    ('S_COLOR_YELLOW "WARNING: x"', "Com_Warning"),
    ('"Warning: x"', "Com_Warning"),
    ('"^1Fatal: x"', "Com_PrintError"),
    ('"Error loading %s"', "Com_Printf"),
    ('"\\nWARNING: x"', "Com_Printf"),
    ('S_COLOR_RED "no word"', "Com_Printf"),
]
CALLS = [
    ('ri.Printf(printParm_t::PRINT_WARNING, "WARNING: R_X %s\\n", name);', 'Com_Warning("R_X %s\\n", name);'),
    ('ri.Printf(printParm_t::PRINT_ALL, "x %d\\n", n);', 'Com_Printf("x %d\\n", n);'),
    ('ri.Printf(printParm_t::PRINT_DEVELOPER, S_COLOR_RED "d\\n");', 'Com_DPrintf(S_COLOR_RED "d\\n");'),
    ('trap->Print(S_COLOR_RED "ERROR: %s\\n", x);', 'Com_PrintError("%s\\n", x);'),
    ('trap->Error(errorParm_t::ERR_DROP, "e %d", n);', 'Com_Error(errorParm_t::ERR_DROP, "e %d", n);'),
    ('botimport.Print(printType_t::PRT_FATAL, "AAS: %d\\n", n);', 'Com_PrintError("AAS: %d\\n", n);'),
    ('botimport.Print(printType_t::PRT_WARNING, "w\\n");', 'Com_Warning("w\\n");'),
    ('void QDECL AAS_Error(const char* fmt, ...)', 'void QDECL AAS_Error(const char* fmt, ...)'),
    ('Com_Printf(S_COLOR_YELLOW "WARNING: f(%s)\\n", g(a, b));', 'Com_Warning("f(%s)\\n", g(a, b));'),
    ('// trap->Print("c\\n");', '// Com_Printf("c\\n");'),
]

SP_CALLS = [
    ('G_Error("bad %d", n);', 'Com_Error(errorParm_t::ERR_DROP, "bad %d", n);'),
    ('::G_Error("bad %d", n);', 'Com_Error(errorParm_t::ERR_DROP, "bad %d", n);'),
    ('CG_Error("CG_X: %s", s);', 'Com_Error(errorParm_t::ERR_DROP, "CG_X: %s", s);'),
    ('gi.Printf(S_COLOR_YELLOW "WARNING: x %d\\n", n);', 'Com_Warning("x %d\\n", n);'),
    ('CG_Printf("y\\n");', 'Com_Printf("y\\n");'),
    ('gi.Error(errorParm_t::ERR_DROP, "z");', 'Com_Error(errorParm_t::ERR_DROP, "z");'),
]

bad = 0
for text, want in SP_CALLS:
    got = convert.convert(text, convert.SP, "t", [])
    if got != want:
        print("sp call", text, "->", got, "want", want)
        bad += 1
for arg, want in STRIP:
    got = convert.strip_level(arg)
    if got != want:
        print("strip", arg, "->", got, "want", want)
        bad += 1
for arg, want in TARGET:
    got = convert.target_of_message(arg)
    if got != want:
        print("target", arg, "->", got, "want", want)
        bad += 1
for text, want in CALLS:
    got = convert.convert(text, convert.MP, "t", [])
    if got != want:
        print("call", text, "->", got, "want", want)
        bad += 1
print("convert_test:", "ok" if bad == 0 else f"{bad} failures")
sys.exit(1 if bad else 0)

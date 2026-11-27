/*
===========================================================================
Copyright (C) 2026, ModernJK contributors

This file is part of the ModernJK source code.

ModernJK is free software; you can redistribute it and/or modify it
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

// engine.cpp -- the platform layer as an engine runs it (engine.h): one platform::Window and one platform::Input,
// opened from the display's settings and kept until the program ends; the layer's events turned into the engine's
// own: a key of the player's layout into the engine's key code, a typed character into the engine's one-byte set,
// the mouse's motion as it is. Example: on an AZERTY keyboard the key
// marked Z arrives with the symbol 'z' and becomes A_CAP_Z, which "bind z" names; the same key typed in the chat
// arrives as the character 'z'.
// The engine calls free functions, so what they share is one object, made when the window opens and reached through
// one function (Running); every other function here is given what it works on.
#include "engine.h"

#include <client/client.h>
#include <qcommon/q_version.h>
#include <qcommon/qcommon.h>

#include <shared/platform/input/input.h>
#include <shared/platform/menus/display.h>
#include <shared/platform/platform.h>
#include <shared/platform/window/window.h>
#include <shared/sys/sys_public.h>

#include "window_icon.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace {

// the engine's key codes of the accented letters and signs its one-byte character set shares with Unicode
constexpr char32_t kLowAGrave = 0xE0;
constexpr char32_t kLowThorn = 0xFE;
constexpr char32_t kLowYDiaeresis = 0xFF;
constexpr char32_t kMultiplySign = 0xD7;
constexpr char32_t kDivisionSign = 0xF7;
constexpr char32_t kLastAscii = 0x7E;
// the signs of the one-byte set that have a key code of their own beside the letters
constexpr char32_t kInvertedExclamation = 0xA1;
constexpr char32_t kCentSign = 0xA2;
constexpr char32_t kPoundSign = 0xA3;
constexpr char32_t kYenSign = 0xA5;
constexpr char32_t kCopyrightSign = 0xA9;
constexpr char32_t kRegisteredSign = 0xAE;
constexpr char32_t kInvertedQuestion = 0xBF;
constexpr char32_t kSharpS = 0xDF;
// the characters a key can be named by: beyond ASCII, up to Unicode's last
constexpr char32_t kLastUnicode = 0x10FFFF;
// the mouse's buttons beyond the five named ones go to the engine's auxiliary keys, which are this many
constexpr int kAuxiliaryKeys = 32;

// The display's settings, each registered once and never null after.
struct Settings {
    cvar_t* displayState;
    cvar_t* screen;
    cvar_t* windowWidth;
    cvar_t* windowHeight;
    cvar_t* exclusiveMode;
    cvar_t* swapInterval;
};

// What the engine's functions share from the window's opening to the program's end.
struct Layer {
    Layer(platform::Window&& opened, const platform::WindowSettings& applied, const Settings& registered)
        : window(std::move(opened)), input(window), settings(applied), cvars(registered), screens(platform::Screens(screenModes))
    {
    }

    platform::Window window;
    // made from the window, so declared after it
    platform::Input input;
    // the display's settings as the window has them
    platform::WindowSettings settings;
    Settings cvars;
    // the screens as the menus list them, read again when they change; their modes, which the system enumerates slowly
    // the first time, only once the menus ask for them (Platform_Screens)
    platform::ScreenModes screenModes = platform::ScreenModes::Skipped;
    std::vector<platform::Screen> screens;
    // the key pressed last, so that the character of the key that opens the console is not typed into it
    fakeAscii_t lastKeyDown = fakeAscii_t::A_NULL;
    // when the input was last read
    platform::Time inputTime{};
    // the system changed the window's size in pixels since the last frame (a screen of another scale, a compositor)
    bool resized = false;
    // what the system refused last time, so that a refusal repeated at every frame is told once
    bool mouseRefused = false;
    bool confineRefused = false;
    bool textRefused = false;
    bool presentRefused = false;
};

// The one layer of the program, empty until the window opens and after the program's end.
std::optional<Layer>& Running()
{
    static std::optional<Layer> layer;
    return layer;
}

// The control character of a letter, as a terminal makes it: Ctrl-H is 8.
constexpr int Control(char letter)
{
    return letter - 'a' + 1;
}

// Tells a refusal of the system once: when it starts, not at every frame it lasts.
void Report(bool done, bool& refused, const char* what)
{
    if (!done && !refused) {
        Q_PrintlnWarning("{}", what);
    }

    refused = !done;
}

// Registers the display's settings, applied when they change (Platform_Frame).
Settings RegisterSettings()
{
    return {.displayState = Cvar_Get("r_displayState", "1", cvarFlags_t::CVAR_ARCHIVE),
            .screen = Cvar_Get("r_screen", "0", cvarFlags_t::CVAR_ARCHIVE),
            .windowWidth = Cvar_Get("r_windowWidth", "1280", cvarFlags_t::CVAR_ARCHIVE),
            .windowHeight = Cvar_Get("r_windowHeight", "720", cvarFlags_t::CVAR_ARCHIVE),
            .exclusiveMode = Cvar_Get("r_exclusiveMode", "", cvarFlags_t::CVAR_ARCHIVE),
            .swapInterval = Cvar_Get("r_swapInterval", "0", cvarFlags_t::CVAR_ARCHIVE_ND)};
}

// Whether one of the settings the window takes changed since the call before; their flags cleared.
bool TakeModified(const Settings& cvars)
{
    bool modified = false;

    for (cvar_t* const cvar : {cvars.displayState, cvars.screen, cvars.windowWidth, cvars.windowHeight, cvars.exclusiveMode}) {
        if (cvar->modified) {
            modified = true;
            cvar->modified = false;
        }
    }

    return modified;
}

// The swap r_swapInterval names: 0 at once, a negative value the adaptive sync, any other the vertical sync.
platform::SwapInterval SwapInterval(int setting)
{
    if (setting == 0) {
        return platform::SwapInterval::Immediate;
    }

    return setting < 0 ? platform::SwapInterval::AdaptiveSync : platform::SwapInterval::VerticalSync;
}

// The display's settings as the window takes them.
platform::WindowSettings ReadSettings(const Settings& cvars)
{
    return {.state = platform::DisplayStateOf(cvars.displayState->integer),
            .screen = cvars.screen->integer,
            .width = cvars.windowWidth->integer,
            .height = cvars.windowHeight->integer,
            .exclusive = platform::ParseMode(cvars.exclusiveMode->string)};
}

// Applies the display's settings to the window when they differ from what it has: true when they did, the window
// then perhaps of another size.
bool ApplySettings(Layer& layer)
{
    const platform::WindowSettings wanted = ReadSettings(layer.cvars);

    if (wanted == layer.settings) {
        return false;
    }

    if (!layer.window.Apply(wanted)) {
        Q_PrintlnWarning("the display's settings could not be applied");
    }

    layer.settings = wanted;
    return true;
}

// The engine's key of one the layer names.
fakeAscii_t NamedKeyCode(platform::NamedKey key)
{
    using enum platform::NamedKey;

    switch (key) {
    case Escape:
        return fakeAscii_t::A_ESCAPE;
    case Enter:
        return fakeAscii_t::A_ENTER;
    case Tab:
        return fakeAscii_t::A_TAB;
    case Backspace:
        return fakeAscii_t::A_BACKSPACE;
    case Delete:
        return fakeAscii_t::A_DELETE;
    case Insert:
        return fakeAscii_t::A_INSERT;
    case Home:
        return fakeAscii_t::A_HOME;
    case End:
        return fakeAscii_t::A_END;
    case PageUp:
        return fakeAscii_t::A_PAGE_UP;
    case PageDown:
        return fakeAscii_t::A_PAGE_DOWN;
    case Up:
        return fakeAscii_t::A_CURSOR_UP;
    case Down:
        return fakeAscii_t::A_CURSOR_DOWN;
    case Left:
        return fakeAscii_t::A_CURSOR_LEFT;
    case Right:
        return fakeAscii_t::A_CURSOR_RIGHT;
    case F1:
        return fakeAscii_t::A_F1;
    case F2:
        return fakeAscii_t::A_F2;
    case F3:
        return fakeAscii_t::A_F3;
    case F4:
        return fakeAscii_t::A_F4;
    case F5:
        return fakeAscii_t::A_F5;
    case F6:
        return fakeAscii_t::A_F6;
    case F7:
        return fakeAscii_t::A_F7;
    case F8:
        return fakeAscii_t::A_F8;
    case F9:
        return fakeAscii_t::A_F9;
    case F10:
        return fakeAscii_t::A_F10;
    case F11:
        return fakeAscii_t::A_F11;
    case F12:
        return fakeAscii_t::A_F12;
    case LeftShift:
        return fakeAscii_t::A_SHIFT;
    case RightShift:
        return fakeAscii_t::A_SHIFT2;
    case LeftCtrl:
        return fakeAscii_t::A_CTRL;
    case RightCtrl:
        return fakeAscii_t::A_CTRL2;
    case LeftAlt:
        return fakeAscii_t::A_ALT;
    case RightAlt:
        return fakeAscii_t::A_ALT2;
    case CapsLock:
        return fakeAscii_t::A_CAPSLOCK;
    case NumLock:
        return fakeAscii_t::A_NUMLOCK;
    case ScrollLock:
        return fakeAscii_t::A_SCROLLLOCK;
    case Pause:
        return fakeAscii_t::A_PAUSE;
    case PrintScreen:
        return fakeAscii_t::A_PRINTSCREEN;
    case Keypad0:
        return fakeAscii_t::A_KP_0;
    case Keypad1:
        return fakeAscii_t::A_KP_1;
    case Keypad2:
        return fakeAscii_t::A_KP_2;
    case Keypad3:
        return fakeAscii_t::A_KP_3;
    case Keypad4:
        return fakeAscii_t::A_KP_4;
    case Keypad5:
        return fakeAscii_t::A_KP_5;
    case Keypad6:
        return fakeAscii_t::A_KP_6;
    case Keypad7:
        return fakeAscii_t::A_KP_7;
    case Keypad8:
        return fakeAscii_t::A_KP_8;
    case Keypad9:
        return fakeAscii_t::A_KP_9;
    case KeypadPeriod:
        return fakeAscii_t::A_KP_PERIOD;
    case KeypadEnter:
        return fakeAscii_t::A_KP_ENTER;
    case KeypadPlus:
        return fakeAscii_t::A_KP_PLUS;
    case KeypadMinus:
        return fakeAscii_t::A_KP_MINUS;
    case KeypadMultiply:
        return fakeAscii_t::A_MULTIPLY;
    case KeypadDivide:
        return fakeAscii_t::A_DIVIDE;
    case None:
        break;
    }

    return fakeAscii_t::A_NULL;
}

// The engine's key of a character of the player's layout. The engine's key codes follow its one-byte character
// set: a letter is its capital's code, an accented letter of that set its capital's, any other printable ASCII
// character its own code.
fakeAscii_t SymbolKeyCode(char32_t symbol)
{
    if (symbol >= U'a' && symbol <= U'z') {
        return fakeAscii_t::A_CAP_A + static_cast<int>(symbol - U'a');
    }

    // the accented letters, from a grave to thorn, but the division sign among them
    if (symbol >= kLowAGrave && symbol <= kLowThorn && symbol != kDivisionSign) {
        return fakeAscii_t::A_CAP_AGRAVE + static_cast<int>(symbol - kLowAGrave);
    }

    if (symbol >= U' ' && symbol <= kLastAscii) {
        return static_cast<fakeAscii_t>(symbol);
    }

    switch (symbol) {
    case kMultiplySign:
        return fakeAscii_t::A_MULTIPLY;
    case kDivisionSign:
        return fakeAscii_t::A_DIVIDE;
    case kLowYDiaeresis:
        return fakeAscii_t::A_LOW_YDIERESIS;
    case kInvertedExclamation:
        return fakeAscii_t::A_EXCLAMDOWN;
    case kCentSign:
        return fakeAscii_t::A_CENT;
    case kPoundSign:
        return fakeAscii_t::A_POUND;
    case kYenSign:
        return fakeAscii_t::A_YEN;
    case kCopyrightSign:
        return fakeAscii_t::A_COPYRIGHT;
    case kRegisteredSign:
        return fakeAscii_t::A_REGISTERED;
    case kInvertedQuestion:
        return fakeAscii_t::A_QUESTION_DOWN;
    case kSharpS:
        return fakeAscii_t::A_GERMANDBLS;
    default:
        return fakeAscii_t::A_NULL;
    }
}

// The name the engine writes in a bind for a key it has no code of its own for: the character the layout gives the
// key as "U+" and its Unicode number, so that the name is the same on every system and drawn by the game's font;
// else the system's name of the key, in capitals, its spaces as underscores. Empty when the system has no name of
// plain letters for it.
std::string UnlistedKeyName(const platform::KeyEvent& event)
{
    if (event.symbol != U'\0') {
        return fmt::format("U+{:04X}", static_cast<std::uint32_t>(event.symbol));
    }

    std::string name = platform::KeyName(event.id);

    for (char& letter : name) {
        const auto byte = static_cast<unsigned char>(letter);

        if (byte > kLastAscii) {
            return {};
        }

        letter = letter == ' ' ? '_' : static_cast<char>(std::toupper(byte));
    }

    return name;
}

// Whether a key of the keypad types its digit: with num lock, in the console, the chat and the menus; in game, and
// while the UI waits for a key to bind, the keypad's keys stay keys, so that their binds work.
bool KeypadTypesDigits(const platform::KeyEvent& event)
{
    static const cvar_t* const com_waitingForKey = Cvar_Get("com_waitingForKey", "0", cvarFlags_t::CVAR_TEMP);
    const bool digit = event.named >= platform::NamedKey::Keypad0 && event.named <= platform::NamedKey::Keypad9;
    return digit && event.numLock && Any(Key_GetCatcher()) && com_waitingForKey->integer == 0;
}

// The engine's key of a key event; A_NULL for a key the engine has no code for.
fakeAscii_t KeyCode(const platform::KeyEvent& event)
{
    // the key under Escape opens and closes the console; with Shift it is the key its layout marks
    if (event.console && !kg.keys[fakeAscii_t::A_SHIFT].down && !kg.keys[fakeAscii_t::A_SHIFT2].down) {
        return fakeAscii_t::A_CONSOLE;
    }

    if (event.named == platform::NamedKey::None) {
        const fakeAscii_t listed = SymbolKeyCode(event.symbol);

        if (listed != fakeAscii_t::A_NULL) {
            return listed;
        }

        // a key the engine's table has no code for: one of the codes it keeps free, by the key's name
        const std::string name = UnlistedKeyName(event);
        return name.empty() ? fakeAscii_t::A_NULL : Key_DynamicKey(name.c_str()).value_or(fakeAscii_t::A_NULL);
    }

    if (KeypadTypesDigits(event)) {
        return fakeAscii_t::A_0 + (std::to_underlying(event.named) - std::to_underlying(platform::NamedKey::Keypad0));
    }

    return NamedKeyCode(event.named);
}

// The engine's key of a mouse button: the left, the right and the middle one are its first three.
fakeAscii_t ButtonKeyCode(int button)
{
    switch (button) {
    case platform::kLeftButton:
        return fakeAscii_t::A_MOUSE1;
    case platform::kMiddleButton:
        return fakeAscii_t::A_MOUSE3;
    case platform::kRightButton:
        return fakeAscii_t::A_MOUSE2;
    case platform::kBackButton:
        return fakeAscii_t::A_MOUSE4;
    case platform::kForwardButton:
        return fakeAscii_t::A_MOUSE5;
    default:
        return fakeAscii_t::A_AUX0 + (std::max(button - platform::kForwardButton - 1, 0) % kAuxiliaryKeys);
    }
}

// A time of the layer's clock in milliseconds, as the engine counts.
int Milliseconds(platform::Time time)
{
    return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
}

// A key event to the client, with the characters the keys themselves make for the console and the fields:
// Backspace as Ctrl-H, and Ctrl with a letter as that control character.
void OnKey(Layer& layer, const platform::KeyEvent& event)
{
    const fakeAscii_t key = KeyCode(event);

    if (key != fakeAscii_t::A_NULL) {
        CL_KeyEvent(key, event.down, event.time.count());
    }

    if (!event.down) {
        layer.lastKeyDown = fakeAscii_t::A_NULL;
        return;
    }

    const bool ctrl = kg.keys[fakeAscii_t::A_CTRL].down || kg.keys[fakeAscii_t::A_CTRL2].down;

    if (key == fakeAscii_t::A_BACKSPACE) {
        CL_CharEvent(Control('h'));
    }
    else if (ctrl && key >= fakeAscii_t::A_CAP_A && key <= fakeAscii_t::A_CAP_Z) {
        CL_CharEvent(Control(static_cast<char>('a' + (key - fakeAscii_t::A_CAP_A))));
    }

    layer.lastKeyDown = key;
}

// A typed character to the client, in the engine's one-byte set; but not the character of the key that has just
// opened the console.
void OnText(const Layer& layer, const platform::TextEvent& event)
{
    if (layer.lastKeyDown != fakeAscii_t::A_CONSOLE) {
        CL_CharEvent(ConvertUTF32ToExpectedCharset(static_cast<std::uint32_t>(event.character)));
    }
}

// The wheel to the client: a press and a release of the wheel's key for each notch.
void OnWheel(const platform::MouseWheel& event)
{
    const fakeAscii_t key = event.notches > 0 ? fakeAscii_t::A_MWHEELUP : fakeAscii_t::A_MWHEELDOWN;

    for (int notch = 0; notch < std::abs(event.notches); ++notch) {
        CL_KeyEvent(key, true, event.time.count());
        CL_KeyEvent(key, false, event.time.count());
    }
}

// What happened to the window, to the engine's settings, to the sound and to the screens' list.
void OnWindow(Layer& layer, const platform::WindowEvent& event)
{
    switch (event.change) {
    case platform::WindowChange::FocusGained:
        Cvar_SetValue("com_unfocused", 0);
        SND_SetFocus(true);
        break;
    case platform::WindowChange::FocusLost:
        Cvar_SetValue("com_unfocused", 1);
        SND_SetFocus(false);
        break;
    case platform::WindowChange::Minimized:
        Cvar_SetValue("com_minimized", 1);
        break;
    case platform::WindowChange::Restored:
        Cvar_SetValue("com_minimized", 0);
        break;
    case platform::WindowChange::Quit:
        Cbuf_ExecuteText(cbufExec_t::EXEC_NOW, "quit Closed window\n");
        break;
    case platform::WindowChange::ScreensChanged:
        layer.screens = platform::Screens(layer.screenModes);
        break;
    case platform::WindowChange::PixelSizeChanged:
        layer.resized = true;
        break;
    }
}

// One function per kind of event, chosen by the event's type: a kind added to the layer does not compile until it
// has its own here.
class Dispatcher {
public:
    explicit Dispatcher(Layer& layer) : layer_(&layer) {}

    void operator()(const platform::KeyEvent& key) const { OnKey(*layer_, key); }
    void operator()(const platform::TextEvent& text) const { OnText(*layer_, text); }
    // the motion as the device counts it, fractions included
    void operator()(const platform::MouseMotion& motion) const { CL_MouseEvent(motion.dx, motion.dy); }
    // the system's pointer, for the menus
    void operator()(const platform::PointerMotion& motion) const { CL_PointerEvent(motion.x, motion.y); }
    void operator()(const platform::MouseButton& button) const { CL_KeyEvent(ButtonKeyCode(button.button), button.down, button.time.count()); }
    void operator()(const platform::MouseWheel& wheel) const { OnWheel(wheel); }
    void operator()(const platform::WindowEvent& change) const { OnWindow(*layer_, change); }

private:
    // never null: made from a reference
    Layer* layer_;
};

// The refresh rate of the display as it is set, in whole images per second: the mode's in exclusive fullscreen when
// the setting names one, the desktop's otherwise; 0 when the system does not tell.
int RefreshRate(std::span<const platform::Screen> screens, const platform::WindowSettings& settings)
{
    const platform::Screen* const screen = platform::ScreenOf(screens, settings.screen);

    if (screen == nullptr) {
        return 0;
    }

    const bool exclusive = settings.state == platform::DisplayState::Exclusive && settings.exclusive.refresh > 0.0F;
    const float rate = exclusive ? settings.exclusive.refresh : screen->desktop.refresh;
    // to the nearest whole number, a rate never being negative
    const int whole = static_cast<int>(rate);
    return rate - static_cast<float>(whole) >= 0.5F ? whole + 1 : whole;
}

// The modelist command: each screen by the number r_screen takes, with its desktop's mode and the modes exclusive
// fullscreen can set, each written as r_exclusiveMode takes it.
void ModeList_f()
{
    const std::vector<platform::Screen> screens = platform::Screens();

    for (std::size_t number = 0; number < screens.size(); ++number) {
        const platform::Screen& screen = screens[number];
        Q_Println("screen {} \"{}\": desktop {}x{}@{}", number, screen.name, screen.desktop.width, screen.desktop.height, screen.desktop.refresh);

        for (const platform::DisplayMode& mode : screen.modes) {
            Q_Println("  {}x{}@{}", mode.width, mode.height, mode.refresh);
        }
    }
}

// The minimize command.
void Minimize_f()
{
    std::optional<Layer>& running = Running();

    if (running && !running->window.Minimize()) {
        Q_PrintlnWarning("the window could not be minimized");
    }
}

} // namespace

void Platform_OpenWindow()
{
    if (Running()) {
        return;
    }

    if (const auto started = platform::Start(CLIENT_WINDOW_TITLE, GIT_TAG); !started) {
        Q_Error(errorParm_t::ERR_FATAL, "{}", started.error());
    }

    const Settings cvars = RegisterSettings();
    const platform::WindowSettings settings = ReadSettings(cvars);
    auto opened = platform::Window::Open(CLIENT_WINDOW_TITLE, settings, platform::Context::OpenGl);

    if (!opened) {
        Q_Error(errorParm_t::ERR_FATAL, "{}", opened.error());
    }

    Layer& layer = Running().emplace(std::move(*opened), settings, cvars);
    const std::size_t iconBytes = std::size_t{CLIENT_WINDOW_ICON.width} * CLIENT_WINDOW_ICON.height * CLIENT_WINDOW_ICON.bytes_per_pixel;
    const platform::Image icon{.rgba = std::span(CLIENT_WINDOW_ICON.pixel_data).first(iconBytes),
                               .size = {.width = static_cast<int>(CLIENT_WINDOW_ICON.width), .height = static_cast<int>(CLIENT_WINDOW_ICON.height)}};

    if (!layer.window.SetIcon(icon)) {
        Q_DPrintln("the window's icon could not be set");
    }

    // applied at the first image shown
    cvars.swapInterval->modified = true;
    Cmd_AddCommand("modelist", ModeList_f);
    Cmd_AddCommand("minimize", Minimize_f);
    Q_Println("{}", platform::Description());
}

void Platform_Shutdown()
{
    Running().reset();
    platform::Stop();
}

void Platform_Frame()
{
    std::optional<Layer>& running = Running();

    if (!running) {
        return;
    }

    Layer& layer = *running;
    // the mouse turns the view with the focus and when the client says so (in game, while nothing takes the pointer);
    // everywhere else the system's pointer is free, in and out of the window
    const bool view = com_unfocused->integer == 0 && CL_MouseTurnsView();
    const platform::MouseMode mode = view ? platform::MouseMode::Game : platform::MouseMode::Pointer;
    Report(layer.input.SetMouseMode(mode), layer.mouseRefused, "the mouse could not be given to the view or released");
    // in exclusive fullscreen the pointer stays on the game's screen: a click on another screen would take the focus,
    // and the system then minimizes the game. In a window and in borderless fullscreen it is free to leave.
    const bool confined = layer.settings.state == platform::DisplayState::Exclusive;
    Report(layer.input.SetPointerConfined(confined), layer.confineRefused, "the pointer could not be kept in the window or freed");
    // text is typed where something takes characters: the console, the chat, a menu, and the console's line while
    // disconnected
    const bool text = Any(Key_GetCatcher()) || cls.state == connstate_t::CA_DISCONNECTED;
    Report(layer.input.SetTextInput(text), layer.textRefused, "the text input could not be turned on or off");

    const Dispatcher dispatch(layer);
    const std::span<const platform::Event> events = layer.input.Poll();
    // read after the events: none of them is later
    layer.inputTime = platform::Now();

    for (const platform::Event& event : events) {
        std::visit(dispatch, event);
    }

    // after the events, so that Alt+Enter and the menus' Apply take effect at this frame: a display's setting changed
    // (the console, a configuration, the menus) applied to the window, or the window's size changed by the system;
    // the client then draws the next image at the window's size
    const bool applied = TakeModified(layer.cvars) && ApplySettings(layer);
    const bool resized = std::exchange(layer.resized, false);

    if (applied || resized) {
        CL_DisplayChanged();
    }
}

bool Platform_IsKeyName(const char* name)
{
    if (name == nullptr) {
        return false;
    }

    const std::string_view text(name);

    if (text.starts_with("U+")) {
        std::uint32_t character = 0;
        const char* const end = text.data() + text.size();
        const std::from_chars_result parsed = std::from_chars(text.data() + 2, end, character, 16);
        return parsed.ec == std::errc{} && parsed.ptr == end && character > kLastAscii && character <= kLastUnicode;
    }

    std::string spaced(text);
    std::ranges::replace(spaced, '_', ' ');
    return platform::KeyFromName(spaced).has_value();
}

bool Platform_SetPointerImage(std::span<const std::uint8_t> rgba, int width, int height, int shownWidth, int shownHeight)
{
    std::optional<Layer>& running = Running();
    const platform::Image image{.rgba = rgba, .size = {.width = width, .height = height}};
    return running && running->window.SetPointerImage(image, {.width = shownWidth, .height = shownHeight}, {});
}

std::span<const platform::Screen> Platform_Screens()
{
    std::optional<Layer>& running = Running();

    if (!running) {
        return {};
    }

    Layer& layer = *running;

    if (layer.screenModes == platform::ScreenModes::Skipped) {
        layer.screenModes = platform::ScreenModes::Listed;
        layer.screens = platform::Screens(platform::ScreenModes::Listed);
    }

    return layer.screens;
}

int Platform_Milliseconds()
{
    return Milliseconds(platform::Now());
}

std::int64_t Platform_Nanoseconds()
{
    return platform::Now().count();
}

std::int64_t Platform_InputNanoseconds()
{
    const std::optional<Layer>& running = Running();
    return running ? running->inputTime.count() : platform::Now().count();
}

void Platform_Display(glconfig_s* glConfig)
{
    std::optional<Layer>& running = Running();

    if (!running || glConfig == nullptr) {
        return;
    }

    Layer& layer = *running;

    // the renderer starts, or asks again once the display changed: a setting changed since the last frame (set with a
    // restart of the video in one command) is applied to the window first, which stays the same one
    layer.screens = platform::Screens(layer.screenModes);
    ApplySettings(layer);

    const platform::PixelSize size = layer.window.Size();
    glConfig->vidWidth = size.width;
    glConfig->vidHeight = size.height;
    glConfig->displayFrequency = RefreshRate(layer.screens, layer.settings);
    glConfig->isFullscreen = layer.settings.state != platform::DisplayState::Window;
    glConfig->stereoEnabled = false;
    // the screen's own brightness table is never touched: the renderer applies r_gamma itself
    glConfig->deviceSupportsGamma = false;
}

void Platform_Present()
{
    std::optional<Layer>& running = Running();

    if (!running) {
        return;
    }

    Layer& layer = *running;

    if (layer.cvars.swapInterval->modified) {
        layer.cvars.swapInterval->modified = false;

        if (!layer.window.SetSwapInterval(SwapInterval(layer.cvars.swapInterval->integer))) {
            Q_PrintlnWarning("r_swapInterval {} refused by the system", layer.cvars.swapInterval->integer);
        }
    }

    Report(layer.window.Present(), layer.presentRefused, "the image could not be shown");
}

Platform_GlFunction Platform_GlProcAddress(const char* name)
{
    const std::optional<Layer>& running = Running();
    return running && name != nullptr ? running->window.GlProcAddress(name) : nullptr;
}

bool Platform_GlExtensionSupported(const char* name)
{
    const std::optional<Layer>& running = Running();
    return running && name != nullptr && running->window.GlExtensionSupported(name);
}

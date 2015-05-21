#ifndef KEY_EVENT_HPP
#define KEY_EVENT_HPP

#include "glfw.hpp"
#include "string.hpp"

const int KeyModShift   = GLFW_MOD_SHIFT;
const int KeyModControl = GLFW_MOD_CONTROL;
const int KeyModAlt     = GLFW_MOD_ALT;
const int KeyModSuper   = GLFW_MOD_SUPER;

enum KeyAction {
    KeyActionDown,
    KeyActionUp,
};

enum VirtKey {
    VirtKeyUnknown = GLFW_KEY_UNKNOWN,
    VirtKeySpace = GLFW_KEY_SPACE,
    VirtKeyApostrophe = GLFW_KEY_APOSTROPHE,
    VirtKeyComma = GLFW_KEY_COMMA,
    VirtKeyMinus = GLFW_KEY_MINUS,
    VirtKeyPeriod = GLFW_KEY_PERIOD,
    VirtKeySlash = GLFW_KEY_SLASH,
    VirtKey0 = GLFW_KEY_0,
    VirtKey1 = GLFW_KEY_1,
    VirtKey2 = GLFW_KEY_2,
    VirtKey3 = GLFW_KEY_3,
    VirtKey4 = GLFW_KEY_4,
    VirtKey5 = GLFW_KEY_5,
    VirtKey6 = GLFW_KEY_6,
    VirtKey7 = GLFW_KEY_7,
    VirtKey8 = GLFW_KEY_8,
    VirtKey9 = GLFW_KEY_9,
    VirtKeySemicolon = GLFW_KEY_SEMICOLON,
    VirtKeyEqual = GLFW_KEY_EQUAL,
    VirtKeyA = GLFW_KEY_A,
    VirtKeyB = GLFW_KEY_B,
    VirtKeyC = GLFW_KEY_C,
    VirtKeyD = GLFW_KEY_D,
    VirtKeyE = GLFW_KEY_E,
    VirtKeyF = GLFW_KEY_F,
    VirtKeyG = GLFW_KEY_G,
    VirtKeyH = GLFW_KEY_H,
    VirtKeyI = GLFW_KEY_I,
    VirtKeyJ = GLFW_KEY_J,
    VirtKeyK = GLFW_KEY_K,
    VirtKeyL = GLFW_KEY_L,
    VirtKeyM = GLFW_KEY_M,
    VirtKeyN = GLFW_KEY_N,
    VirtKeyO = GLFW_KEY_O,
    VirtKeyP = GLFW_KEY_P,
    VirtKeyQ = GLFW_KEY_Q,
    VirtKeyR = GLFW_KEY_R,
    VirtKeyS = GLFW_KEY_S,
    VirtKeyT = GLFW_KEY_T,
    VirtKeyU = GLFW_KEY_U,
    VirtKeyV = GLFW_KEY_V,
    VirtKeyW = GLFW_KEY_W,
    VirtKeyX = GLFW_KEY_X,
    VirtKeyY = GLFW_KEY_Y,
    VirtKeyZ = GLFW_KEY_Z,
    VirtKeyLeftBracket = GLFW_KEY_LEFT_BRACKET,
    VirtKeyBackslash = GLFW_KEY_BACKSLASH,
    VirtKeyRightBracket = GLFW_KEY_RIGHT_BRACKET,
    VirtKeyGraveAccent = GLFW_KEY_GRAVE_ACCENT,
    VirtKeyWorld1 = GLFW_KEY_WORLD_1,
    VirtKeyWorld2 = GLFW_KEY_WORLD_2,
    VirtKeyEscape = GLFW_KEY_ESCAPE,
    VirtKeyEnter = GLFW_KEY_ENTER,
    VirtKeyTab = GLFW_KEY_TAB,
    VirtKeyBackspace = GLFW_KEY_BACKSPACE,
    VirtKeyInsert = GLFW_KEY_INSERT,
    VirtKeyDelete = GLFW_KEY_DELETE,
    VirtKeyRight = GLFW_KEY_RIGHT,
    VirtKeyLeft = GLFW_KEY_LEFT,
    VirtKeyDown = GLFW_KEY_DOWN,
    VirtKeyUp = GLFW_KEY_UP,
    VirtKeyPageUp = GLFW_KEY_PAGE_UP,
    VirtKeyPageDown = GLFW_KEY_PAGE_DOWN,
    VirtKeyHome = GLFW_KEY_HOME,
    VirtKeyEnd = GLFW_KEY_END,
    VirtKeyCapsLock = GLFW_KEY_CAPS_LOCK,
    VirtKeyScrollLock = GLFW_KEY_SCROLL_LOCK,
    VirtKeyNumLock = GLFW_KEY_NUM_LOCK,
    VirtKeyPrintScreen = GLFW_KEY_PRINT_SCREEN,
    VirtKeyPause = GLFW_KEY_PAUSE,
    VirtKeyF1 = GLFW_KEY_F1,
    VirtKeyF2 = GLFW_KEY_F2,
    VirtKeyF3 = GLFW_KEY_F3,
    VirtKeyF4 = GLFW_KEY_F4,
    VirtKeyF5 = GLFW_KEY_F5,
    VirtKeyF6 = GLFW_KEY_F6,
    VirtKeyF7 = GLFW_KEY_F7,
    VirtKeyF8 = GLFW_KEY_F8,
    VirtKeyF9 = GLFW_KEY_F9,
    VirtKeyF10 = GLFW_KEY_F10,
    VirtKeyF11 = GLFW_KEY_F11,
    VirtKeyF12 = GLFW_KEY_F12,
    VirtKeyF13 = GLFW_KEY_F13,
    VirtKeyF14 = GLFW_KEY_F14,
    VirtKeyF15 = GLFW_KEY_F15,
    VirtKeyF16 = GLFW_KEY_F16,
    VirtKeyF17 = GLFW_KEY_F17,
    VirtKeyF18 = GLFW_KEY_F18,
    VirtKeyF19 = GLFW_KEY_F19,
    VirtKeyF20 = GLFW_KEY_F20,
    VirtKeyF21 = GLFW_KEY_F21,
    VirtKeyF22 = GLFW_KEY_F22,
    VirtKeyF23 = GLFW_KEY_F23,
    VirtKeyF24 = GLFW_KEY_F24,
    VirtKeyF25 = GLFW_KEY_F25,
    VirtKeyKp0 = GLFW_KEY_KP_0,
    VirtKeyKp1 = GLFW_KEY_KP_1,
    VirtKeyKp2 = GLFW_KEY_KP_2,
    VirtKeyKp3 = GLFW_KEY_KP_3,
    VirtKeyKp4 = GLFW_KEY_KP_4,
    VirtKeyKp5 = GLFW_KEY_KP_5,
    VirtKeyKp6 = GLFW_KEY_KP_6,
    VirtKeyKp7 = GLFW_KEY_KP_7,
    VirtKeyKp8 = GLFW_KEY_KP_8,
    VirtKeyKp9 = GLFW_KEY_KP_9,
    VirtKeyKpDecimal = GLFW_KEY_KP_DECIMAL,
    VirtKeyKpDivide = GLFW_KEY_KP_DIVIDE,
    VirtKeyKpMultiply = GLFW_KEY_KP_MULTIPLY,
    VirtKeyKpSubtract = GLFW_KEY_KP_SUBTRACT,
    VirtKeyKpAdd = GLFW_KEY_KP_ADD,
    VirtKeyKpEnter = GLFW_KEY_KP_ENTER,
    VirtKeyKpEqual = GLFW_KEY_KP_EQUAL,
    VirtKeyLeftShift = GLFW_KEY_LEFT_SHIFT,
    VirtKeyLeftControl = GLFW_KEY_LEFT_CONTROL,
    VirtKeyLeftAlt = GLFW_KEY_LEFT_ALT,
    VirtKeyLeftSuper = GLFW_KEY_LEFT_SUPER,
    VirtKeyRightShift = GLFW_KEY_RIGHT_SHIFT,
    VirtKeyRightControl = GLFW_KEY_RIGHT_CONTROL,
    VirtKeyRightAlt = GLFW_KEY_RIGHT_ALT,
    VirtKeyRightSuper = GLFW_KEY_RIGHT_SUPER,
    VirtKeyMenu = GLFW_KEY_MENU,
};

struct KeyEvent {
    KeyAction action;
    VirtKey virt_key;
    int modifiers;
};

struct TextInputEvent {
    uint32_t codepoint;
    int modifiers;
};

static inline bool key_mod_shift(int mods) {
    return mods & KeyModShift;
}

static inline bool key_mod_ctrl(int mods) {
    return mods & KeyModControl;
}

static inline bool key_mod_alt(int mods) {
    return mods & KeyModAlt;
}

static inline bool key_mod_super(int mods) {
    return mods & KeyModSuper;
}

static inline bool key_mod_only_shift(int mods) {
    return key_mod_shift(mods) && !key_mod_ctrl(mods) && !key_mod_alt(mods) && !key_mod_super(mods);
}

static inline bool key_mod_only_ctrl(int mods) {
    return key_mod_ctrl(mods) && !key_mod_shift(mods) && !key_mod_alt(mods) && !key_mod_super(mods);
}

static inline bool key_mod_only_alt(int mods) {
    return !key_mod_ctrl(mods) && !key_mod_shift(mods) && key_mod_alt(mods) && !key_mod_super(mods);
}

static inline bool key_mod_only_super(int mods) {
    return !key_mod_ctrl(mods) && !key_mod_shift(mods) && !key_mod_alt(mods) && key_mod_super(mods);
}

String virt_key_to_string(VirtKey key);

struct KeySequence {
    int modifiers;
    VirtKey key;
};

bool null_key_sequence(const KeySequence &seq);
String key_sequence_to_string(const KeySequence &seq);
bool key_sequence_match(const KeySequence &seq, const KeyEvent *event);

static inline KeySequence make_shortcut(int modifiers, VirtKey key) {
    return {
        .modifiers = modifiers,
        .key = key,
    };
}

static inline KeySequence no_shortcut() { return make_shortcut(-1, VirtKeyUnknown); }
static inline KeySequence alt_shortcut(VirtKey key) { return make_shortcut(KeyModAlt, key); }
static inline KeySequence ctrl_shortcut(VirtKey key) { return make_shortcut(KeyModControl, key); }
static inline KeySequence shift_shortcut(VirtKey key) { return make_shortcut(KeyModShift, key); }
static inline KeySequence ctrl_shift_shortcut(VirtKey key) { return make_shortcut(KeyModControl|KeyModShift, key); }
static inline KeySequence shortcut(VirtKey key) { return make_shortcut(0, key); }


#endif

#include QMK_KEYBOARD_H

#include "kb_machine.h"
#include "kb_protocol.h"
#include "kb_rules.h"
#include "tmux_context.h"
#ifdef RAW_ENABLE
#    include "raw_hid.h"
#endif

enum layers {
    _MAC,
    _LINUX,
    _NUM_MAC,
    _NUM_LINUX,
    _NAV_MAC,
    _NAV_LINUX,
    // tmux mode. _TMUX is the shared base and is on whenever tmux mode is on,
    // with exactly one of the mode layers above it. All of them are shared by
    // both OS modes, because the tmux prefix is Ctrl on Mac and Linux alike.
    _TMUX,
    _TMUX_TREE,
    _TMUX_WINDOW,
    _TMUX_PANE,
    _TMUX_WITHIN,
};

// Numbered from SAFE_RANGE in KB_KEYS order, so subtracting the base gives the
// kb_key_t kb_key takes.
enum custom_keycodes {
    KB_KEYCODE_BASE = SAFE_RANGE - 1,
#define KB_KEYCODE(name) name,
    KB_KEYS(KB_KEYCODE)
#undef KB_KEYCODE
    KB_KEYCODE_END,
};

// Thumbs: space = nav, enter = numbers
#define M_SPC LT(_NAV_MAC, KC_SPC)
#define L_SPC LT(_NAV_LINUX, KC_SPC)
#define M_ENT LT(_NUM_MAC, KC_ENT)
#define L_ENT LT(_NUM_LINUX, KC_ENT)

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_MAC] = LAYOUT_split_3x5_3(
  //,---------------------------------------------------------------------.                              ,---------------------------------------------------------------------.
             KC_Q,         KC_W,         KC_E,         KC_R,         KC_T,                                        KC_Y,         KC_U,         KC_I,         KC_O,         KC_P,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
             KC_A, LCTL_T(KC_S), LALT_T(KC_D), LGUI_T(KC_F),         KC_G,                                        KC_H, RGUI_T(KC_J), RALT_T(KC_K), RCTL_T(KC_L),      KC_SCLN,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
             KC_Z,         KC_X,         KC_C,         KC_V,         KC_B,                                        KC_N,         KC_M,      KC_COMM,       KC_DOT,      KC_QUOT,
  //|-------------+-------------+-------------+-------------+-------------+-------------|  |-------------+-------------+-------------+-------------+-------------+-------------|
                                                    KC_LBRC,        M_SPC,      SC_LSPO,         SC_RSPC,        M_ENT,       KC_RBRC
                                            //`-----------------------------------------'  `-----------------------------------------'
  ),

    [_LINUX] = LAYOUT_split_3x5_3(
  //,---------------------------------------------------------------------.                              ,---------------------------------------------------------------------.
             KC_Q,         KC_W,         KC_E,         KC_R,         KC_T,                                        KC_Y,         KC_U,         KC_I,         KC_O,         KC_P,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
             KC_A, LGUI_T(KC_S), LALT_T(KC_D), LCTL_T(KC_F),         KC_G,                                        KC_H, RCTL_T(KC_J), RALT_T(KC_K), RGUI_T(KC_L),      KC_SCLN,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
             KC_Z,         KC_X,         KC_C,         KC_V,         KC_B,                                        KC_N,         KC_M,      KC_COMM,       KC_DOT,      KC_QUOT,
  //|-------------+-------------+-------------+-------------+-------------+-------------|  |-------------+-------------+-------------+-------------+-------------+-------------|
                                                    KC_LBRC,        L_SPC,      SC_LSPO,         SC_RSPC,        L_ENT,       KC_RBRC
                                            //`-----------------------------------------'  `-----------------------------------------'
  ),

    [_NUM_MAC] = LAYOUT_split_3x5_3(
  //,---------------------------------------------------------------------.                              ,---------------------------------------------------------------------.
             KC_7,         KC_8,         KC_9,         KC_0,       KC_GRV,                                     XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,       KC_ESC,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
             KC_4,         KC_5,         KC_6,      KC_MINS,      KC_BSLS,                                     KC_RSFT,      KC_RGUI,      KC_RALT,      KC_RCTL,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
             KC_1,         KC_2,         KC_3,       KC_EQL,      KC_SLSH,                                     XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------+-------------|  |-------------+-------------+-------------+-------------+-------------+-------------|
                                                    XXXXXXX,      XXXXXXX,      KC_LSFT,         XXXXXXX,      _______,       XXXXXXX
                                            //`-----------------------------------------'  `-----------------------------------------'
  ),

    [_NUM_LINUX] = LAYOUT_split_3x5_3(
  //,---------------------------------------------------------------------.                              ,---------------------------------------------------------------------.
             KC_7,         KC_8,         KC_9,         KC_0,       KC_GRV,                                     XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,       KC_ESC,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
             KC_4,         KC_5,         KC_6,      KC_MINS,      KC_BSLS,                                     KC_RSFT,      KC_RCTL,      KC_RALT,      KC_RGUI,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
             KC_1,         KC_2,         KC_3,       KC_EQL,      KC_SLSH,                                     XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------+-------------|  |-------------+-------------+-------------+-------------+-------------+-------------|
                                                    XXXXXXX,      XXXXXXX,      KC_LSFT,         XXXXXXX,      _______,       XXXXXXX
                                            //`-----------------------------------------'  `-----------------------------------------'
  ),

    [_NAV_MAC] = LAYOUT_split_3x5_3(
  //,---------------------------------------------------------------------.                              ,---------------------------------------------------------------------.
          OS_SWAP,      XXXXXXX,      XXXXXXX,      XXXXXXX,      TMUX_ON,                                      KC_TAB,      KC_BSPC,        KC_UP,       KC_DEL,       KC_ESC,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          XXXXXXX,      KC_LCTL,      KC_LALT,      KC_LGUI,      KC_LSFT,                                   G(KC_TAB),      KC_LEFT,      KC_DOWN,      KC_RGHT,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,                                     G(KC_Z),      G(KC_X),      G(KC_C),      G(KC_V),      G(KC_F),
  //|-------------+-------------+-------------+-------------+-------------+-------------|  |-------------+-------------+-------------+-------------+-------------+-------------|
                                                    XXXXXXX,      _______,      XXXXXXX,         KC_RSFT,      XXXXXXX,       XXXXXXX
                                            //`-----------------------------------------'  `-----------------------------------------'
  ),

    [_NAV_LINUX] = LAYOUT_split_3x5_3(
  //,---------------------------------------------------------------------.                              ,---------------------------------------------------------------------.
          OS_SWAP,      XXXXXXX,      XXXXXXX,      XXXXXXX,      TMUX_ON,                                      KC_TAB,      KC_BSPC,        KC_UP,       KC_DEL,       KC_ESC,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          XXXXXXX,      KC_LGUI,      KC_LALT,      KC_LCTL,      KC_LSFT,                                   C(KC_TAB),      KC_LEFT,      KC_DOWN,      KC_RGHT,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,                                     KC_UNDO,       KC_CUT,      KC_COPY,     KC_PASTE,      KC_FIND,
  //|-------------+-------------+-------------+-------------+-------------+-------------|  |-------------+-------------+-------------+-------------+-------------+-------------|
                                                    XXXXXXX,      _______,      XXXXXXX,         KC_RSFT,      XXXXXXX,       XXXXXXX
                                            //`-----------------------------------------'  `-----------------------------------------'
  ),

    // The mode keys and EXIT live here alone, so every mode layer leaves them
    // transparent and they work identically wherever you are.
    [_TMUX] = LAYOUT_split_3x5_3(
  //,---------------------------------------------------------------------.                              ,---------------------------------------------------------------------.
          XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      TM_EXIT,                                     XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          TM_TREE,       TM_WIN,      TM_PANE,    TM_WITHIN,      XXXXXXX,                                     XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,                                     XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------+-------------|  |-------------+-------------+-------------+-------------+-------------+-------------|
                                                    XXXXXXX,      XXXXXXX,      XXXXXXX,         XXXXXXX,      XXXXXXX,       XXXXXXX
                                            //`-----------------------------------------'  `-----------------------------------------'
  ),

    // Tree mode drives choose-tree's own key table, so most of it is plain keys
    // sent straight through. Alt -/+ are fold all / unfold all; bare -/+ are
    // only aliases for Left/Right, which J and L already cover.
    [_TMUX_TREE] = LAYOUT_split_3x5_3(
  //,---------------------------------------------------------------------.                              ,---------------------------------------------------------------------.
          XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      _______,                                     XXXXXXX,   A(KC_MINS),        KC_UP,   A(KC_PLUS),      TT_KILL,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          _______,      _______,      _______,      _______,      _______,                                     XXXXXXX,      KC_LEFT,      KC_DOWN,      KC_RGHT,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,                                     XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------+-------------|  |-------------+-------------+-------------+-------------+-------------+-------------|
                                                    XXXXXXX,      XXXXXXX,      XXXXXXX,         XXXXXXX,       TT_SEL,       XXXXXXX
                                            //`-----------------------------------------'  `-----------------------------------------'
  ),

    [_TMUX_WINDOW] = LAYOUT_split_3x5_3(
  //,---------------------------------------------------------------------.                              ,---------------------------------------------------------------------.
          XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      _______,                                     TW_LAST,      XXXXXXX,        TW_UP,      XXXXXXX,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          _______,      _______,      _______,      _______,      _______,                                     XXXXXXX,      TW_LEFT,      TW_DOWN,      TW_RGHT,      TM_DTCH,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,                                     XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------+-------------|  |-------------+-------------+-------------+-------------+-------------+-------------|
                                                    XXXXXXX,      XXXXXXX,      TW_MOVE,         XXXXXXX,      XXXXXXX,       XXXXXXX
                                            //`-----------------------------------------'  `-----------------------------------------'
  ),

    [_TMUX_PANE] = LAYOUT_split_3x5_3(
  //,---------------------------------------------------------------------.                              ,---------------------------------------------------------------------.
          XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      _______,                                     TP_LAST,      TP_ZOOM,        TP_UP,      XXXXXXX,       TP_LYT,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          _______,      _______,      _______,      _______,      _______,                                     XXXXXXX,      TP_LEFT,      TP_DOWN,      TP_RGHT,      TM_DTCH,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,                                     XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,
  //|-------------+-------------+-------------+-------------+-------------+-------------|  |-------------+-------------+-------------+-------------+-------------+-------------|
                                                    TP_RSZE,      TP_SPLT,      TP_MOVE,         XXXXXXX,      XXXXXXX,       XXXXXXX
                                            //`-----------------------------------------'  `-----------------------------------------'
  ),

    // Within mode navigates inside whatever the focused pane is running. Not one
    // of these keys has a fixed meaning: within_resolve decides what each sends
    // from the host's report, so the same L is a vi w in copy mode, the next
    // prompt in Claude's transcript, and nothing at all at a live Claude prompt.
    // There is no key here that says which program it is -- the keyboard is told.
    [_TMUX_WITHIN] = LAYOUT_split_3x5_3(
  //,---------------------------------------------------------------------.                              ,---------------------------------------------------------------------.
          XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      _______,                                     TC_NEXT,      TC_MARK,        TC_UP,       TC_SEL,     TC_LEAVE,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          _______,      _______,      _______,      _______,      _______,                                     TC_PREV,      TC_LEFT,      TC_DOWN,      TC_RGHT,      TC_BKGD,
  //|-------------+-------------+-------------+-------------+-------------|                              |-------------+-------------+-------------+-------------+-------------|
          XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,      XXXXXXX,                                     XXXXXXX,      XXXXXXX,      TC_COPY,      TC_PSTE,      TC_SRCH,
  //|-------------+-------------+-------------+-------------+-------------+-------------|  |-------------+-------------+-------------+-------------+-------------+-------------|
                                                    TC_WORD,      TC_LINE,      XXXXXXX,         XXXXXXX,      XXXXXXX,       XXXXXXX
                                            //`-----------------------------------------'  `-----------------------------------------'
  )
};
// clang-format on

// The layer above _TMUX for each mode.
static uint8_t mode_layer(uint8_t mode) {
    switch (mode) {
        case KB_MODE_TREE:
            return _TMUX_TREE;
        case KB_MODE_WINDOW:
            return _TMUX_WINDOW;
        case KB_MODE_WITHIN:
            return _TMUX_WITHIN;
        default:
            return _TMUX_PANE;
    }
}

uint32_t kb_io_now_ms(void) {
    return timer_read32();
}

void kb_io_send(const uint8_t report[KB_REPORT_SIZE], const kb_state_t *state) {
    (void)state;
#ifdef RAW_ENABLE
    raw_hid_send((uint8_t *)report, KB_REPORT_SIZE);
#else
    (void)report;
#endif
}

void kb_io_set_mode_layers(uint8_t mode) {
    layer_off(_TMUX_TREE);
    layer_off(_TMUX_WINDOW);
    layer_off(_TMUX_PANE);
    layer_off(_TMUX_WITHIN);
    if (mode == KB_MODE_OFF) {
        layer_off(_TMUX);
    } else {
        layer_on(_TMUX);
        layer_on(mode_layer(mode));
    }
}

void kb_io_tap_row(uint8_t fkey, uint8_t mods) {
    // register_mods ignores an empty mask and sends the report itself, so the
    // modifier is down in the same report as the function key rather than beside
    // it, which is the whole point of sending it this way.
    register_mods(mods);
    tap_code(fkey);
    unregister_mods(mods);
}

void kb_io_tap_code(uint8_t keycode) {
    tap_code(keycode);
}

void kb_io_tap_code16(uint16_t keycode) {
    tap_code16(keycode);
}

void kb_io_set_base(uint8_t os) {
    set_single_persistent_default_layer(os == KB_OS_LINUX ? _LINUX : _MAC);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    kb_key_t key = (keycode > KB_KEYCODE_BASE && keycode < KB_KEYCODE_END) ? keycode - KB_KEYCODE_BASE : KBK_OTHER;
    return kb_key(key, record->event.pressed);
}

#ifdef RAW_ENABLE
void raw_hid_receive(uint8_t *data, uint8_t length) {
    kb_receive(data, length);
}
#endif

void keyboard_post_init_user(void) {
    kb_machine_init(get_highest_layer(default_layer_state) == _LINUX ? KB_OS_LINUX : KB_OS_MAC);
    kb_hello();
}

#ifdef OLED_ENABLE
bool oled_task_user(void) {
    if (is_keyboard_master()) {
        // The display is 21 characters by four lines and every one is spoken
        // for, so the OS label shares its line with the OS rather than having
        // one of its own.
        oled_write_P(PSTR("OS "), false);
        oled_write_P(get_highest_layer(default_layer_state) == _LINUX ? PSTR("LINUX") : PSTR("MAC"), false);
        // Which OS is only half the story once the host reports one: LOCK is
        // what says this layer is a decision rather than a report, and that
        // OS_SWAP is what will give it back.
        if (kb.os_source != KB_OS_FOLLOW) {
            oled_write_P(PSTR(" LOCK"), false);
        }
        oled_write_ln_P(PSTR(""), false);
        // The tmux layers are toggled, so without this there is no way to tell
        // which mode - or whether tmux mode at all - is on.
        switch (kb.mode) {
            case KB_MODE_TREE:
                oled_write_P(PSTR("TREE"), false);
                break;
            case KB_MODE_WINDOW:
                oled_write_P(PSTR("WINDOW"), false);
                break;
            case KB_MODE_PANE:
                oled_write_P(PSTR("PANE"), false);
                break;
            // WITHIN prints the target rather than the mode. The mode is always
            // WITHIN and says nothing; the target is what decides what every key
            // on the layer sends, so it is the only thing worth the line.
            case KB_MODE_WITHIN:
                switch (kb_within_now()) {
                    case WT_TRANSCRIPT:
                        oled_write_P(PSTR("TRSC"), false);
                        break;
                    case WT_CLAUDE_SAFE:
                        oled_write_P(PSTR("CLAUDE"), false);
                        break;
                    case WT_HUNK:
                        oled_write_P(PSTR("HUNK"), false);
                        break;
                    default:
                        oled_write_P(PSTR("COPY"), false);
                        break;
                }
                break;
        }
        switch (kb.mod) {
            case TMOD_RESIZE:
                oled_write_P(PSTR(" RESIZE"), false);
                break;
            case TMOD_SPLIT:
                oled_write_P(PSTR(" SPLIT"), false);
                break;
            case TMOD_MOVE:
                oled_write_P(PSTR(" MOVE"), false);
                break;
            case TMOD_WORD:
                oled_write_P(PSTR(" WORD"), false);
                break;
            case TMOD_LINE:
                oled_write_P(PSTR(" LINE"), false);
                break;
        }
        // ? is "waiting to hear", for the two things the keyboard asks for and
        // cannot see the answer to: an intent the host is still working on, and a
        // Ctrl-O whose viewer the host has not reported yet. ! is a failed
        // intent, for a second.
        if (kb.ctx.intent_status == KB_INTENT_PENDING || (kb.mode == KB_MODE_WITHIN && kb_within_now() == WT_CLAUDE_SAFE && kb.ctx.transcript == KB_TRANSCRIPT_UNKNOWN)) {
            oled_write_P(PSTR(" ?"), false);
        } else if (kb.intent_failed && timer_elapsed32(kb.intent_mark_ms) < 1000) {
            oled_write_P(PSTR(" !"), false);
        }
        // No fresh report from the host, so the keyboard has no idea what is in
        // the pane: WITHIN is plain copy mode and nothing resolves. Worth a mark
        // of its own, because every key still works and only the smart ones are
        // missing.
        if (kb.mode != KB_MODE_OFF && !kb_alive()) {
            oled_write_P(PSTR(" ~"), false);
        }
        // A kill is waiting on its second press. tmux is showing its own prompt
        // too, but that is down in the status line and easy to miss.
        if (kb.kill_pending) {
            oled_write_P(PSTR(" KILL?"), false);
        }
        // Pads out the rest of the line, which is also what clears it when tmux
        // mode is off.
        oled_write_ln_P(PSTR(""), false);
        // What Claude is doing, and what it is allowed to do without asking.
        // Neither is guessable from the keyboard and both change what the next
        // keypress is worth: there is no point stepping through a transcript
        // that is still being written, and bypassPermissions is worth seeing
        // before you background something. Blank for any other program, because
        // the line is only meaningful for Claude.
        if (kb_alive() && kb.ctx.program == KB_PROGRAM_CLAUDE) {
            switch (kb.ctx.phase) {
                case KB_PHASE_IDLE:
                    oled_write_P(PSTR("idle"), false);
                    break;
                case KB_PHASE_RUNNING:
                    oled_write_P(PSTR("run"), false);
                    break;
                case KB_PHASE_WAITING:
                    oled_write_P(PSTR("wait"), false);
                    break;
            }
            switch (kb.ctx.perm) {
                case KB_PERM_DEFAULT:
                    oled_write_P(PSTR(" default"), false);
                    break;
                case KB_PERM_PLAN:
                    oled_write_P(PSTR(" plan"), false);
                    break;
                case KB_PERM_ACCEPT_EDITS:
                    oled_write_P(PSTR(" accept"), false);
                    break;
                case KB_PERM_AUTO:
                    oled_write_P(PSTR(" auto"), false);
                    break;
                case KB_PERM_BYPASS:
                    oled_write_P(PSTR(" bypass"), false);
                    break;
                case KB_PERM_DONT_ASK:
                    oled_write_P(PSTR(" dontask"), false);
                    break;
            }
        }
        oled_write_ln_P(PSTR(""), false);
        oled_write_ln_P(is_caps_word_on() ? PSTR("CAPS") : PSTR(""), false);
    }
    return false;
}
#endif

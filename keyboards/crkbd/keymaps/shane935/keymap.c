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

enum custom_keycodes {
    OS_SWAP = SAFE_RANGE,
    // Mode switching, shared by every tmux layer.
    TMUX_ON,
    TM_TREE,
    TM_WIN,
    TM_PANE,
    TM_WITHIN,
    TM_EXIT,
    TM_DTCH,
    // PANE mode.
    TP_UP,
    TP_DOWN,
    TP_LEFT,
    TP_RGHT,
    TP_LAST,
    TP_ZOOM,
    TP_LYT,
    TP_RSZE,
    TP_SPLT,
    TP_MOVE,
    // WINDOW mode.
    TW_UP,
    TW_DOWN,
    TW_LEFT,
    TW_RGHT,
    TW_LAST,
    TW_MOVE,
    // TREE mode.
    TT_SEL,
    TT_KILL,
    // WITHIN mode. What each of these sends is not written here or in the
    // layer: it comes out of within_resolve, from what the host reports is in
    // the pane. The same key is a vi motion in copy mode, a prompt jump in
    // Claude's transcript, and nothing at all at a live Claude prompt.
    TC_UP,
    TC_DOWN,
    TC_LEFT,
    TC_RGHT,
    TC_SRCH,
    TC_NEXT,
    TC_PREV,
    TC_LEAVE,
    TC_BKGD,
    TC_MARK,
    TC_SEL,
    TC_COPY,
    TC_PSTE,
    TC_WORD,
    TC_LINE,
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

// Killing raises tmux's own "(y/n)" prompt, and the tree layer has no y on it
// to answer with. So a kill is two presses of the same key: one to ask, one to
// answer. True between those two presses.
static bool tmux_kill_pending = false;

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

void kb_io_send(const uint8_t report[KB_REPORT_SIZE]) {
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

void kb_io_set_base(uint8_t os) {
    set_single_persistent_default_layer(os == KB_OS_LINUX ? _LINUX : _MAC);
}

// Every tmux action is one row of the key table in docs/00-protocol.md: a
// single root-table key, which tmux runs the instant it arrives. The commands
// themselves -- and so the directory a new pane opens in, and every other flag
// -- live in the generated tmux.conf, not here. mods names the row, so S-F19 in
// the table is tmux_fkey(KC_F19, MOD_LSFT) here.
static void tmux_fkey(uint8_t fkey, uint8_t mods) {
    // register_mods ignores an empty mask and sends the report itself, so the
    // modifier is down in the same report as the function key rather than beside
    // it, which is the whole point of sending it this way.
    register_mods(mods);
    tap_code(fkey);
    unregister_mods(mods);
    // The host never actuates this -- tmux already has. It says which row went
    // out so the host knows what to watch for, and in the same encoding the
    // table uses: the row number, plus a bit per modifier.
    kb.event     = KB_EVENT_TMUX_KEY;
    kb.event_arg = kb_tmux_event_arg(fkey - KC_F13 + 1, mods & MOD_LSFT, mods & MOD_LCTL, mods & MOD_LALT);
    kb_send_state(0, 0);
}

// TREE and WITHIN put the pane into a real tmux mode rather than just changing
// what the keyboard sends, so the pane has to be taken back out of it before
// anything else happens.
static void tmux_quit_mode(void) {
    if (kb.mode == KB_MODE_TREE || kb.mode == KB_MODE_WITHIN) {
        tmux_fkey(KC_F20, 0);
    }
}

// The host mirrors the held modifier into its status line, so it has to hear
// about the release as well as the press.
static void tmux_set_mod(uint8_t mod) {
    kb.mod = mod;
    kb_send_state(0, 0);
}

// The four directions differ only in which row they send, and the modifiers
// only in which mod rides on it: Shift resizes, Ctrl splits, Alt swaps. That is
// the whole of the table's PANE block.
static void tmux_pane_arrow(uint8_t fkey) {
    switch (kb.mod) {
        case TMOD_RESIZE:
            tmux_fkey(fkey, MOD_LSFT);
            break;
        case TMOD_SPLIT:
            tmux_fkey(fkey, MOD_LCTL);
            break;
        case TMOD_MOVE:
            tmux_fkey(fkey, MOD_LALT);
            break;
        default:
            tmux_fkey(fkey, 0);
            break;
    }
}

static within_mod_t within_mod_now(void) {
    return kb.mod == TMOD_WORD ? WM_WORD : kb.mod == TMOD_LINE ? WM_LINE : WM_NONE;
}

// Sends one resolved action. A row of the tmux key table goes out through
// tmux_fkey so the host still hears the event; anything else is a keystroke for
// the program. An action that is all zero sends nothing, which is the point of
// it -- that is claude-safe refusing to put a letter into a live prompt.
static void within_send(within_key_t key) {
    within_action_t a = within_resolve(kb_within_now(), key, within_mod_now());

    if (a.intent) {
        kb.intent_failed = false;
        kb.intent_acked  = KB_INTENT_NONE;
        kb_send_state(a.intent, 0);
        return;
    }
    const uint16_t pair[2] = {a.key, a.then};
    for (int i = 0; i < 2; i++) {
        if (pair[i] == KC_NO) {
            continue;
        }
        if (kb_is_tmux_row(pair[i])) {
            tmux_fkey(kb_tmux_row_key(pair[i]), kb_tmux_row_mods(pair[i]));
        } else {
            tap_code16(pair[i]);
        }
    }
}

static void tmux_switch_mode(uint8_t mode) {
    tmux_quit_mode();
    kb_set_mode(mode);
    if (mode == KB_MODE_TREE) {
        tmux_fkey(KC_F21, 0);
    } else if (mode == KB_MODE_WITHIN) {
        // Whatever the target needs to become navigable: copy-mode for a shell,
        // Ctrl-O for a Claude pane whose viewer is shut, nothing for a viewer or
        // for hunk, which are navigable already.
        within_send(WK_ENTER);
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // The kill prompt is tmux's, not the keyboard's: while it is up it swallows
    // the next key whatever that key was meant for. So every key except the one
    // that answers it says no first and then does its own job, and the prompt
    // can never outlive the press that raised it.
    if (record->event.pressed && tmux_kill_pending && keycode != TT_KILL) {
        tap_code(KC_N);
        tmux_kill_pending = false;
    }

    // Every thumb modifier is held, so these are the only keys that have
    // anything to do on the release.
    switch (keycode) {
        case TP_RSZE:
            tmux_set_mod(record->event.pressed ? TMOD_RESIZE : TMOD_NONE);
            return false;
        case TP_SPLT:
            tmux_set_mod(record->event.pressed ? TMOD_SPLIT : TMOD_NONE);
            return false;
        case TP_MOVE:
        case TW_MOVE:
            tmux_set_mod(record->event.pressed ? TMOD_MOVE : TMOD_NONE);
            return false;
        // WITHIN's two are held like PANE's: one unit per thumb, and nothing
        // survives letting go.
        case TC_WORD:
            tmux_set_mod(record->event.pressed ? TMOD_WORD : TMOD_NONE);
            return false;
        case TC_LINE:
            tmux_set_mod(record->event.pressed ? TMOD_LINE : TMOD_NONE);
            return false;
    }

    if (!record->event.pressed) {
        return true;
    }

    switch (keycode) {
        // Follow the host, force Mac, force Linux, follow the host again. The
        // first tap off Follow may not move the layer at all -- it is already
        // where the host put it -- so the OLED says LOCK to show what changed.
        case OS_SWAP:
            kb.os_source = kb_os_next(kb.os_source);
            kb_apply_os();
            kb_send_state(0, 0);
            return false;

        // Tapping the current mode's own key quits TREE or WITHIN and falls back
        // to PANE; in PANE and WINDOW there is nothing open to quit.
        case TMUX_ON:
            kb_set_mode(KB_MODE_PANE);
            return false;
        case TM_TREE:
            tmux_switch_mode(kb.mode == KB_MODE_TREE ? KB_MODE_PANE : KB_MODE_TREE);
            return false;
        case TM_WIN:
            if (kb.mode != KB_MODE_WINDOW) {
                tmux_switch_mode(KB_MODE_WINDOW);
            }
            return false;
        case TM_PANE:
            if (kb.mode != KB_MODE_PANE) {
                tmux_switch_mode(KB_MODE_PANE);
            }
            return false;
        // F goes deeper while there is deeper to go and comes back out when there
        // is not: on a Claude pane it opens the viewer, then asks for the whole
        // conversation in copy mode, then leaves. In copy mode or hunk there is
        // nothing further in, so it leaves straight away -- which is also what
        // stops it closing and reopening copy mode and losing the cursor.
        case TM_WITHIN:
            if (kb.mode == KB_MODE_WITHIN) {
                switch (kb_within_now()) {
                    // Copy mode is as far in as it goes, so F comes out of it.
                    // A Claude pane has the viewer underneath, so that lands back
                    // in the viewer and WITHIN still means something. Anything
                    // else has nothing underneath, and WITHIN with nothing open
                    // sends vi letters to the program, so the mode comes out too.
                    case WT_COPY:
                        if (kb_alive() && kb.ctx.program == KB_PROGRAM_CLAUDE) {
                            tmux_fkey(KC_F20, 0);
                        } else {
                            tmux_switch_mode(KB_MODE_PANE);
                        }
                        return false;
                    // Nothing was opened, so there is nothing to unwind.
                    case WT_HUNK:
                        return false;
                    default:
                        break;
                }
            }
            tmux_switch_mode(KB_MODE_WITHIN);
            return false;
        case TM_EXIT:
            // Leaving a tree or copy mode open would send the next thing typed
            // into it instead of the shell.
            tmux_quit_mode();
            kb_set_mode(KB_MODE_OFF);
            return false;
        case TM_DTCH:
            tmux_fkey(KC_F17, MOD_LCTL);
            kb_set_mode(KB_MODE_OFF);
            return false;

        case TP_UP:
            tmux_pane_arrow(KC_F15);
            return false;
        case TP_DOWN:
            tmux_pane_arrow(KC_F14);
            return false;
        case TP_LEFT:
            tmux_pane_arrow(KC_F13);
            return false;
        case TP_RGHT:
            tmux_pane_arrow(KC_F16);
            return false;
        case TP_LAST:
            tmux_fkey(KC_F17, 0);
            return false;
        case TP_ZOOM:
            tmux_fkey(KC_F18, 0);
            return false;
        case TP_LYT:
            tmux_fkey(KC_F18, MOD_LSFT);
            return false;

        // Up and down are sessions, which MOVE has nothing to say about, so
        // they are dead while it is held.
        case TW_UP:
            if (kb.mod == TMOD_NONE) {
                tmux_fkey(KC_F19, MOD_LSFT);
            }
            return false;
        case TW_DOWN:
            if (kb.mod == TMOD_NONE) {
                tmux_fkey(KC_F20, MOD_LSFT);
            }
            return false;
        case TW_LEFT:
            tmux_fkey(KC_F22, kb.mod == TMOD_MOVE ? MOD_LSFT : 0);
            return false;
        case TW_RGHT:
            tmux_fkey(KC_F23, kb.mod == TMOD_MOVE ? MOD_LSFT : 0);
            return false;
        case TW_LAST:
            tmux_fkey(KC_F21, MOD_LSFT);
            return false;

        case TT_SEL:
            // The tree's own template decides what choosing an item means, so
            // there is nothing to know here beyond "the tree is now closed".
            tap_code(KC_ENT);
            kb_set_mode(KB_MODE_PANE);
            return false;
        // A bare x, because the tree is reading keys itself rather than through
        // the prefix. It kills whatever is highlighted, session or window, and
        // the tree stays open afterwards.
        case TT_KILL:
            if (tmux_kill_pending) {
                tap_code(KC_Y);
            } else {
                tap_code(KC_X);
            }
            tmux_kill_pending = !tmux_kill_pending;
            return false;

        // Every one of these is the same two lines: ask the resolver what this
        // key means in whatever the host says is in the pane, and send that. The
        // keymap deliberately holds no opinion -- the tables are in
        // tmux_context.c and the state behind them belongs to the host.
        case TC_UP:
            within_send(WK_UP);
            return false;
        case TC_DOWN:
            within_send(WK_DOWN);
            return false;
        case TC_LEFT:
            within_send(WK_LEFT);
            return false;
        case TC_RGHT:
            within_send(WK_RIGHT);
            return false;
        case TC_SRCH:
            within_send(WK_SEARCH);
            return false;
        case TC_NEXT:
            within_send(WK_NEXT);
            return false;
        case TC_PREV:
            within_send(WK_PREV);
            return false;
        case TC_BKGD:
            within_send(WK_BKGD);
            return false;
        case TC_MARK:
            within_send(WK_MARK);
            return false;
        case TC_SEL:
            within_send(WK_SELECT);
            return false;
        // Leaves what WITHIN opened, without leaving WITHIN: closing Claude's
        // viewer lands back on its prompt, which is still a Claude pane, and the
        // next report says so.
        case TC_LEAVE:
            within_send(WK_LEAVE);
            return false;
        // The two that end the mode as well as doing something. Both are copy
        // mode only, so the mode only moves if the resolver actually sent
        // something -- in the transcript or at a Claude prompt they are dead and
        // the keyboard stays where it is.
        case TC_COPY:
            if (kb_within_now() == WT_COPY) {
                within_send(WK_COPY);
                kb_set_mode(KB_MODE_PANE);
            }
            return false;
        case TC_PSTE:
            if (kb_within_now() == WT_COPY) {
                within_send(WK_PASTE);
                kb_set_mode(KB_MODE_OFF);
            }
            return false;
    }

    return true;
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
        if (tmux_kill_pending) {
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

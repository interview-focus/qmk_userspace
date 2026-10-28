// The keyboard's desired state and the link to the host, with no QMK in it.
//
// Everything the keyboard decides lives here; keymap.c only turns keycodes into
// calls on it and carries out the kb_io_ functions at the bottom. Those are
// declared here and defined by whoever links this file: keymap.c with QMK
// calls, test/kb_machine_test.c with a recorder.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "kb_protocol.h"
#include "tmux_context.h"

// The left thumb modifier in effect. Every mode's is held, and only ever one at
// a time. PANE and WINDOW both call their inner thumb MOVE and neither can be on
// while the other is, so they share a value. These are the values STATE byte 9
// carries, so 4 stays empty rather than being reused: the protocol reserved it
// when WINDOW's NEW thumb went.
enum tmux_modifier {
    TMOD_NONE,
    TMOD_RESIZE,
    TMOD_SPLIT,
    TMOD_MOVE,
    TMOD_WORD = 5,
    TMOD_LINE,
};

// Every key the machine handles, in the order keymap.c numbers its custom
// keycodes from SAFE_RANGE, so a keycode is its kb_key_t plus a constant.
#define KB_KEYS(X) \
    X(OS_SWAP) \
    /* Mode switching, shared by every tmux layer. */ \
    X(TMUX_ON) \
    X(TM_TREE) \
    X(TM_WIN) \
    X(TM_PANE) \
    X(TM_WITHIN) \
    X(TM_EXIT) \
    X(TM_DTCH) \
    /* PANE mode. */ \
    X(TP_UP) \
    X(TP_DOWN) \
    X(TP_LEFT) \
    X(TP_RGHT) \
    X(TP_LAST) \
    X(TP_ZOOM) \
    X(TP_LYT) \
    X(TP_RSZE) \
    X(TP_SPLT) \
    X(TP_MOVE) \
    /* WINDOW mode. */ \
    X(TW_UP) \
    X(TW_DOWN) \
    X(TW_LEFT) \
    X(TW_RGHT) \
    X(TW_LAST) \
    X(TW_MOVE) \
    /* TREE mode. */ \
    X(TT_SEL) \
    X(TT_KILL) \
    /* WITHIN mode. What each of these sends is not written here or in the */ \
    /* layer: it comes out of within_resolve, from what the host reports is */ \
    /* in the pane. The same key is a vi motion in copy mode, a prompt jump */ \
    /* in Claude's transcript, and nothing at all at a live Claude prompt. */ \
    X(TC_UP) \
    X(TC_DOWN) \
    X(TC_LEFT) \
    X(TC_RGHT) \
    X(TC_SRCH) \
    X(TC_NEXT) \
    X(TC_PREV) \
    X(TC_LEAVE) \
    X(TC_BKGD) \
    X(TC_MARK) \
    X(TC_SEL) \
    X(TC_COPY) \
    X(TC_PSTE) \
    X(TC_WORD) \
    X(TC_LINE) \
    /* end */

typedef enum {
    // Any key that is not one of these: it still answers a pending kill prompt.
    KBK_OTHER,
#define KB_KEY_ENUM(name) KBK_##name,
    KB_KEYS(KB_KEY_ENUM)
#undef KB_KEY_ENUM
} kb_key_t;

typedef struct {
    // Which tmux mode is open, as STATE byte 8 numbers it. KB_MODE_TMUX is never
    // used: _TMUX is never on alone.
    uint8_t mode;
    uint8_t mod;
    // An enum kb_os_source, and the base layer it last put the keyboard on.
    uint8_t os_source;
    uint8_t base_os;

    // What the host reports. Reports only: nothing in here is ever a command,
    // and the state above is never overwritten by it except through the rules
    // in kb_receive.
    kb_context_t ctx;
    // When the last CONTEXT landed. Zero until the first, which is what stops
    // the first 1.5 s after boot from reading as alive.
    uint32_t ctx_last_ms;
    uint8_t  my_seq, nonce;

    // What the keyboard just did, for the host to schedule its observation
    // around. It describes one keypress, so kb_send_state clears it.
    uint8_t event, event_arg;

    // Killing raises tmux's own "(y/n)" prompt, and the tree layer has no y on it
    // to answer with. So a kill is two presses of the same key: one to ask, one to
    // answer. True between those two presses.
    bool kill_pending;

    // The intent status already acted on, so a host repeating it does not
    // re-trigger, and when the mark it left on the OLED goes out.
    uint8_t  intent_acked;
    uint32_t intent_mark_ms;
    bool     intent_failed;
} kb_machine_t;

extern kb_machine_t kb;

// base_os is the layer the keyboard came up on, which is persisted.
void kb_machine_init(uint8_t base_os);

// Evaluated when asked and never cached: a mode entered while the host was
// alive can be left after it has died.
bool kb_alive(void);

// Which target WITHIN is driving right now. Resolved from the host at the
// moment of asking, never remembered: the pane under the cursor can change
// without the keyboard being told, and a stale answer is how Escape ends up in
// a live Claude prompt.
within_target_t kb_within_now(void);

// Puts the base layer wherever the source in charge says, which may be nowhere.
void kb_apply_os(void);

// Moves the mode layers, clears the held modifier and says so.
void kb_set_mode(uint8_t mode);

// Sent on every change to a field the keyboard owns, on every intent, and
// whenever a CONTEXT turns out to be answering an older STATE.
void kb_send_state(uint8_t intent, uint8_t arg);

// The first report after connecting.
void kb_hello(void);

// A key going down or up. False when the machine has dealt with it, true when
// QMK should carry on with the keycode as usual.
bool kb_key(kb_key_t key, bool pressed);

// One report from the host. Anything that is not a CONTEXT this firmware
// understands is ignored.
void kb_receive(const uint8_t *data, uint8_t length);

// Defined by whoever links this file.
uint32_t kb_io_now_ms(void);
// The report as it goes over the wire, and the values it was built from.
void     kb_io_send(const uint8_t report[KB_REPORT_SIZE], const kb_state_t *state);
// The mode layers for a KB_MODE_ value, KB_MODE_OFF turning them all off.
void kb_io_set_mode_layers(uint8_t mode);
// A row of the tmux key table: fkey with the MOD_ bits held in the same report.
void kb_io_tap_row(uint8_t fkey, uint8_t mods);
// QMK's tap_code and tap_code16.
void kb_io_tap_code(uint8_t keycode);
void kb_io_tap_code16(uint16_t keycode);
// The persistent base layer for KB_OS_MAC or KB_OS_LINUX.
void kb_io_set_base(uint8_t os);

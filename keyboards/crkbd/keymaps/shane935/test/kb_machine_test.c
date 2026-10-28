// Unit tests for kb_machine.c. Run with test/run.sh.
//
// The kb_io_ functions are defined here to record what the machine asked for,
// in order, so each test checks the exact calls a report or a STATE produced.

#include "../kb_machine.h"
#include "../kb_rules.h"

#include <stdio.h>
#include <string.h>

#include "modifiers.h"
#include "quantum_keycodes.h"

static int failures;

static void check_eq(unsigned got, unsigned want, const char *what) {
    if (got != want) {
        printf("FAIL %s: got %u, want %u\n", what, got, want);
        failures++;
    }
}

// ------------------------------------------------------------------- io

enum call_kind { CALL_SEND, CALL_LAYERS, CALL_BASE, CALL_ROW, CALL_TAP, CALL_TAP16 };

typedef struct {
    enum call_kind kind;
    uint16_t       arg;
    uint8_t        mods;
    uint8_t        report[KB_REPORT_SIZE];
} call_t;

static call_t   calls[16];
static int      n_calls;
static uint32_t now_ms;

static void record(enum call_kind kind, uint16_t arg, const uint8_t *report) {
    if (n_calls == (int)(sizeof calls / sizeof calls[0])) {
        printf("FAIL more calls than the recorder holds\n");
        failures++;
        return;
    }
    calls[n_calls].kind = kind;
    calls[n_calls].arg  = arg;
    calls[n_calls].mods = 0;
    if (report) {
        memcpy(calls[n_calls].report, report, KB_REPORT_SIZE);
    }
    n_calls++;
}

uint32_t kb_io_now_ms(void) {
    return now_ms;
}

void kb_io_send(const uint8_t report[KB_REPORT_SIZE], const kb_state_t *state) {
    (void)state;
    record(CALL_SEND, 0, report);
}

void kb_io_set_mode_layers(uint8_t mode) {
    record(CALL_LAYERS, mode, NULL);
}

void kb_io_set_base(uint8_t os) {
    record(CALL_BASE, os, NULL);
}

void kb_io_tap_row(uint8_t fkey, uint8_t mods) {
    record(CALL_ROW, fkey, NULL);
    calls[n_calls - 1].mods = mods;
}

void kb_io_tap_code(uint8_t keycode) {
    record(CALL_TAP, keycode, NULL);
}

void kb_io_tap_code16(uint16_t keycode) {
    record(CALL_TAP16, keycode, NULL);
}

static void reset(uint8_t base_os) {
    kb_machine_init(base_os);
    n_calls = 0;
    now_ms  = 1000;
}

// ------------------------------------------------------------- helpers

// A CONTEXT as the host would send it, at the offsets kb_context_parse reads.
static void receive(const kb_context_t *c) {
    uint8_t b[KB_REPORT_SIZE] = {KB_MAGIC, KB_VERSION, c->seq, c->ack, KB_CONTEXT, c->os, c->program, c->tmux_bits, c->transcript,
                                 c->phase, c->perm, c->intent_nonce, c->intent_status, c->window, c->pane};
    memcpy(&b[15], c->label, strlen(c->label));
    kb_receive(b, sizeof b);
}

// A CONTEXT from a host that is in tmux and has seen the keyboard's latest
// STATE, so it asks for nothing back.
static kb_context_t steady(void) {
    return (kb_context_t){.seq = 1, .ack = kb.my_seq, .os = KB_OS_MAC, .program = KB_PROGRAM_SHELL, .tmux_bits = KB_TMUX_PRESENT | KB_TMUX_FOCUSED};
}

static unsigned count(enum call_kind kind) {
    unsigned n = 0;
    for (int i = 0; i < n_calls; i++) {
        n += calls[i].kind == kind;
    }
    return n;
}

// ---------------------------------------------------------------- hello

static void test_hello(void) {
    reset(KB_OS_MAC);
    kb_hello();
    check_eq(n_calls, 1, "hello: one report");
    const uint8_t *r = calls[0].report;
    check_eq(r[4], KB_HELLO, "hello: type");
    check_eq(r[2], 1, "hello: first seq");
    check_eq(r[3], KB_SEQ_NONE, "hello: nothing to ack yet");
    check_eq(r[10], KB_OS_MAC, "hello: the base layer it came up on");
    check_eq(r[11], 0, "hello: host not alive");
}

// ------------------------------------------------------------- receive

static void test_rejected_report(void) {
    reset(KB_OS_MAC);
    kb_context_t c = steady();
    uint8_t      b[KB_REPORT_SIZE] = {KB_MAGIC, KB_VERSION + 1, c.seq, c.ack, KB_CONTEXT, KB_OS_LINUX};
    kb_receive(b, sizeof b);
    check_eq(n_calls, 0, "rejected: nothing done");
    check_eq(kb_alive(), false, "rejected: host still not alive");
    check_eq(kb.ctx.seq, KB_SEQ_NONE, "rejected: context untouched");
}

static void test_alive(void) {
    reset(KB_OS_MAC);
    kb_context_t c = steady();
    receive(&c);
    check_eq(kb_alive(), true, "a report makes the host alive");
    now_ms += KB_HOST_TIMEOUT_MS - 1;
    check_eq(kb_alive(), true, "still alive just inside the timeout");
    now_ms += 1;
    check_eq(kb_alive(), false, "dead at the timeout");

    // A report landing at tick zero is stored as one, or it would read as no
    // report at all.
    reset(KB_OS_MAC);
    now_ms = 0;
    receive(&c);
    now_ms = 10;
    check_eq(kb_alive(), true, "a report at tick zero still counts");
}

static void test_ack(void) {
    reset(KB_OS_MAC);
    kb_hello();
    n_calls = 0;

    kb_context_t c = steady();
    receive(&c);
    check_eq(n_calls, 0, "acked the latest STATE: nothing sent");

    c.seq = 2;
    c.ack = 0;
    receive(&c);
    check_eq(n_calls, 1, "acked an older STATE: one resend");
    check_eq(calls[0].report[4], KB_STATE, "resend: type");
    check_eq(calls[0].report[2], 2, "resend: next seq");
    check_eq(calls[0].report[3], 2, "resend: acks the report just received");
    check_eq(calls[0].report[11], KB_FLAG_HOST_ALIVE, "resend: host alive");
}

static void test_follow_os(void) {
    reset(KB_OS_MAC);
    kb_context_t c = steady();
    c.os           = KB_OS_LINUX;
    receive(&c);
    check_eq(count(CALL_BASE), 1, "follow: the host's OS moves the layer");
    check_eq(calls[0].arg, KB_OS_LINUX, "follow: to linux");
    check_eq(kb.base_os, KB_OS_LINUX, "follow: remembered");

    n_calls = 0;
    receive(&c);
    check_eq(count(CALL_BASE), 0, "follow: already there, not set again");

    c.os = KB_OS_UNKNOWN;
    receive(&c);
    check_eq(count(CALL_BASE), 0, "follow: an unknown OS leaves the layer");
}

static void test_forced_os(void) {
    reset(KB_OS_MAC);
    kb.os_source   = KB_OS_FORCE_MAC;
    kb_context_t c = steady();
    c.os           = KB_OS_LINUX;
    receive(&c);
    check_eq(count(CALL_BASE), 0, "forced mac: the host's linux is ignored");

    kb.os_source = KB_OS_FORCE_LINUX;
    kb_apply_os();
    check_eq(count(CALL_BASE), 1, "forced linux: moves the layer");
    check_eq(kb.base_os, KB_OS_LINUX, "forced linux: remembered");
}

static void test_adopt_pane(void) {
    reset(KB_OS_MAC);
    kb.mode        = KB_MODE_TREE;
    kb.mod         = TMOD_MOVE;
    kb_context_t c = steady();
    c.ack          = 0;
    receive(&c);
    check_eq(n_calls, 2, "TREE closed behind the keyboard: layers, then one STATE");
    check_eq(calls[0].kind, CALL_LAYERS, "adopt: layers first");
    check_eq(calls[0].arg, KB_MODE_PANE, "adopt: to PANE");
    check_eq(calls[1].report[8], KB_MODE_PANE, "adopt: STATE says PANE");
    check_eq(calls[1].report[9], TMOD_NONE, "adopt: held modifier cleared");
    check_eq(kb.mode, KB_MODE_PANE, "adopt: mode moved");

    reset(KB_OS_MAC);
    kb.mode          = KB_MODE_TREE;
    c                = steady();
    c.intent_status  = KB_INTENT_PENDING;
    receive(&c);
    check_eq(kb.mode, KB_MODE_TREE, "an intent in flight explains it: TREE kept");

    // WITHIN only claims a mode when it is driving copy mode.
    reset(KB_OS_MAC);
    kb.mode = KB_MODE_WITHIN;
    c       = steady();
    receive(&c);
    check_eq(kb.mode, KB_MODE_PANE, "WITHIN on a shell, copy mode gone: PANE");

    reset(KB_OS_MAC);
    kb.mode      = KB_MODE_WITHIN;
    c            = steady();
    c.program    = KB_PROGRAM_CLAUDE;
    c.transcript = KB_TRANSCRIPT_OPEN;
    receive(&c);
    check_eq(kb.mode, KB_MODE_WITHIN, "WITHIN in Claude's viewer: kept");
}

static void test_intent_result(void) {
    reset(KB_OS_MAC);
    kb_send_state(KB_INTENT_WITHIN_OPEN, 0);
    check_eq(calls[0].report[5], KB_INTENT_WITHIN_OPEN, "intent: sent");
    check_eq(calls[0].report[7], 1, "intent: takes the next nonce");

    kb_context_t c = steady();
    c.intent_nonce  = 0;
    c.intent_status = KB_INTENT_DONE;
    receive(&c);
    check_eq(kb.intent_acked, KB_INTENT_NONE, "an older nonce's answer is ignored");

    now_ms          = 2000;
    c.intent_nonce  = 1;
    receive(&c);
    check_eq(kb.intent_acked, KB_INTENT_DONE, "done: acted on");
    check_eq(kb.intent_failed, false, "done: not failed");
    check_eq(kb.intent_mark_ms, 2000, "done: marked now");

    now_ms = 2500;
    receive(&c);
    check_eq(kb.intent_mark_ms, 2000, "the same answer again: mark not moved");

    now_ms          = 3000;
    c.intent_status = KB_INTENT_FAILED;
    receive(&c);
    check_eq(kb.intent_failed, true, "failed: marked failed");
    check_eq(kb.intent_mark_ms, 3000, "failed: marked now");
}

// --------------------------------------------------------------- state

static void test_state_fields(void) {
    reset(KB_OS_LINUX);
    kb.os_source = KB_OS_FORCE_LINUX;
    kb.mode      = KB_MODE_PANE;
    kb.mod       = TMOD_RESIZE;
    kb.event     = KB_EVENT_TMUX_KEY;
    kb.event_arg = kb_tmux_event_arg(1, true, false, false);
    kb_send_state(0, 0);
    const uint8_t *r = calls[0].report;
    check_eq(r[8], KB_MODE_PANE, "state: mode");
    check_eq(r[9], TMOD_RESIZE, "state: held modifier");
    check_eq(r[10], KB_OS_LINUX | KB_OS_OVERRIDE, "state: base layer with the override bit");
    check_eq(r[12], KB_EVENT_TMUX_KEY, "state: event");
    check_eq(r[13], 0x11, "state: event arg");

    kb_send_state(0, 0);
    check_eq(calls[1].report[12], KB_EVENT_NONE, "the event goes out once");
    check_eq(calls[1].report[13], 0, "and its arg with it");
    check_eq(calls[1].report[2], 2, "seq counts every report");
}

static void test_set_mode(void) {
    reset(KB_OS_MAC);
    kb.mode = KB_MODE_WITHIN;
    kb.mod  = TMOD_WORD;
    kb_set_mode(KB_MODE_OFF);
    check_eq(calls[0].kind, CALL_LAYERS, "set mode: layers first");
    check_eq(calls[0].arg, KB_MODE_OFF, "set mode: off");
    check_eq(calls[1].report[8], KB_MODE_OFF, "set mode: STATE says off");
    check_eq(calls[1].report[9], TMOD_NONE, "set mode: WORD does not survive");
}

// ----------------------------------------------------------------- keys
//
// Each case is checked as a script of what went out, one call per item:
//
//   F15, S-F15     a row of the tmux key table, with its modifiers
//   tap N          tap_code; tap16 S(N) is tap_code16
//   layers PANE    the mode layers moved
//   base LINUX     the base layer moved
//   state          a STATE went to the host

static const char *mode_name(unsigned mode) {
    static const char *names[] = {"OFF", "TMUX", "TREE", "WINDOW", "PANE", "WITHIN"};
    return mode < 6 ? names[mode] : "?";
}

static const char *key_name(unsigned kc) {
    static const struct {
        uint16_t    kc;
        const char *name;
    } names[] = {
        {KC_N, "N"},       {KC_X, "X"},          {KC_Y, "Y"},        {KC_ENT, "ENT"}, {KC_UP, "UP"},  {KC_PGUP, "PGUP"},
        {KC_B, "B"},       {KC_ESC, "ESC"},      {S(KC_N), "S(N)"},  {KC_LEFT, "LEFT"}, {KC_LBRC, "LBRC"},
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        if (names[i].kc == kc) return names[i].name;
    }
    return "?";
}

static void render(char *out, size_t size) {
    size_t at = 0;
    out[0]    = '\0';
    for (int i = 0; i < n_calls && at < size; i++) {
        const call_t *c   = &calls[i];
        const char   *sep = i ? " | " : "";
        switch (c->kind) {
            case CALL_ROW:
                at += snprintf(out + at, size - at, "%s%s%s%sF%u", sep, c->mods & MOD_LSFT ? "S-" : "", c->mods & MOD_LCTL ? "C-" : "",
                               c->mods & MOD_LALT ? "M-" : "", c->arg - KC_F13 + 13);
                break;
            case CALL_TAP:
                at += snprintf(out + at, size - at, "%stap %s", sep, key_name(c->arg));
                break;
            case CALL_TAP16:
                at += snprintf(out + at, size - at, "%stap16 %s", sep, key_name(c->arg));
                break;
            case CALL_LAYERS:
                at += snprintf(out + at, size - at, "%slayers %s", sep, mode_name(c->arg));
                break;
            case CALL_BASE:
                at += snprintf(out + at, size - at, "%sbase %s", sep, c->arg == KB_OS_LINUX ? "LINUX" : "MAC");
                break;
            case CALL_SEND:
                at += snprintf(out + at, size - at, "%sstate", sep);
                break;
        }
    }
}

static void expect(const char *what, const char *want) {
    char got[256];
    render(got, sizeof got);
    if (strcmp(got, want) != 0) {
        printf("FAIL %s:\n  got  %s\n  want %s\n", what, got, want);
        failures++;
    }
}

// The keyboard in a mode with a live host reporting a shell pane, and nothing
// recorded yet.
static void in_mode(uint8_t mode, uint8_t mod) {
    reset(KB_OS_MAC);
    kb_context_t c = steady();
    receive(&c);
    kb.mode = mode;
    kb.mod  = mod;
    n_calls = 0;
}

static void press(kb_key_t key) {
    check_eq(kb_key(key, true), false, "a machine key is handled");
}

static void test_pane_arrows(void) {
    static const struct {
        uint8_t     mod;
        const char *want[4];
    } rows[] = {
        {TMOD_NONE, {"F15 | state", "F14 | state", "F13 | state", "F16 | state"}},
        {TMOD_RESIZE, {"S-F15 | state", "S-F14 | state", "S-F13 | state", "S-F16 | state"}},
        {TMOD_SPLIT, {"C-F15 | state", "C-F14 | state", "C-F13 | state", "C-F16 | state"}},
        {TMOD_MOVE, {"M-F15 | state", "M-F14 | state", "M-F13 | state", "M-F16 | state"}},
    };
    const kb_key_t keys[4] = {KBK_TP_UP, KBK_TP_DOWN, KBK_TP_LEFT, KBK_TP_RGHT};
    for (size_t r = 0; r < sizeof rows / sizeof rows[0]; r++) {
        for (int k = 0; k < 4; k++) {
            in_mode(KB_MODE_PANE, rows[r].mod);
            press(keys[k]);
            expect("PANE arrow", rows[r].want[k]);
        }
    }

    // The STATE after a row says which row went out.
    in_mode(KB_MODE_PANE, TMOD_RESIZE);
    press(KBK_TP_UP);
    check_eq(calls[1].report[12], KB_EVENT_TMUX_KEY, "arrow: STATE carries the event");
    check_eq(calls[1].report[13], kb_tmux_event_arg(3, true, false, false), "arrow: row 3 with shift");
}

static void test_pane_keys(void) {
    in_mode(KB_MODE_PANE, TMOD_NONE);
    press(KBK_TP_LAST);
    expect("TP_LAST", "F17 | state");
    in_mode(KB_MODE_PANE, TMOD_NONE);
    press(KBK_TP_ZOOM);
    expect("TP_ZOOM", "F18 | state");
    in_mode(KB_MODE_PANE, TMOD_NONE);
    press(KBK_TP_LYT);
    expect("TP_LYT", "S-F18 | state");
}

static void test_held_thumbs(void) {
    static const struct {
        kb_key_t key;
        uint8_t  mod;
    } thumbs[] = {
        {KBK_TP_RSZE, TMOD_RESIZE}, {KBK_TP_SPLT, TMOD_SPLIT}, {KBK_TP_MOVE, TMOD_MOVE},
        {KBK_TW_MOVE, TMOD_MOVE},   {KBK_TC_WORD, TMOD_WORD},  {KBK_TC_LINE, TMOD_LINE},
    };
    for (size_t i = 0; i < sizeof thumbs / sizeof thumbs[0]; i++) {
        in_mode(KB_MODE_PANE, TMOD_NONE);
        check_eq(kb_key(thumbs[i].key, true), false, "thumb press handled");
        check_eq(kb.mod, thumbs[i].mod, "thumb press sets its modifier");
        check_eq(calls[0].report[9], thumbs[i].mod, "thumb press: STATE says so");
        check_eq(kb_key(thumbs[i].key, false), false, "thumb release handled");
        check_eq(kb.mod, TMOD_NONE, "thumb release clears it");
        expect("thumb press and release", "state | state");
    }
}

static void test_releases(void) {
    in_mode(KB_MODE_PANE, TMOD_NONE);
    check_eq(kb_key(KBK_TP_UP, false), true, "a release of a tapped key goes on to QMK");
    check_eq(kb_key(KBK_OTHER, true), true, "a key the machine does not know goes on to QMK");
    expect("releases and other keys", "");
}

static void test_window_keys(void) {
    static const struct {
        kb_key_t    key;
        uint8_t     mod;
        const char *want;
    } cases[] = {
        {KBK_TW_UP, TMOD_NONE, "S-F19 | state"},   {KBK_TW_UP, TMOD_MOVE, ""},
        {KBK_TW_DOWN, TMOD_NONE, "S-F20 | state"}, {KBK_TW_DOWN, TMOD_MOVE, ""},
        {KBK_TW_LEFT, TMOD_NONE, "F22 | state"},   {KBK_TW_LEFT, TMOD_MOVE, "S-F22 | state"},
        {KBK_TW_RGHT, TMOD_NONE, "F23 | state"},   {KBK_TW_RGHT, TMOD_MOVE, "S-F23 | state"},
        {KBK_TW_LAST, TMOD_NONE, "S-F21 | state"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        in_mode(KB_MODE_WINDOW, cases[i].mod);
        press(cases[i].key);
        expect("WINDOW key", cases[i].want);
    }
}

static void test_mode_keys(void) {
    in_mode(KB_MODE_OFF, TMOD_NONE);
    press(KBK_TMUX_ON);
    expect("TMUX_ON", "layers PANE | state");

    in_mode(KB_MODE_PANE, TMOD_NONE);
    press(KBK_TM_TREE);
    expect("TM_TREE from PANE opens the tree", "layers TREE | state | F21 | state");

    in_mode(KB_MODE_TREE, TMOD_NONE);
    press(KBK_TM_TREE);
    expect("TM_TREE in TREE quits to PANE", "F20 | state | layers PANE | state");

    in_mode(KB_MODE_TREE, TMOD_NONE);
    press(KBK_TM_WIN);
    expect("TM_WIN from TREE quits the tree first", "F20 | state | layers WINDOW | state");

    in_mode(KB_MODE_WINDOW, TMOD_MOVE);
    press(KBK_TM_WIN);
    expect("TM_WIN in WINDOW does nothing", "");
    check_eq(kb.mod, TMOD_MOVE, "and leaves the thumb alone");

    in_mode(KB_MODE_WINDOW, TMOD_NONE);
    press(KBK_TM_PANE);
    expect("TM_PANE from WINDOW", "layers PANE | state");

    in_mode(KB_MODE_PANE, TMOD_NONE);
    press(KBK_TM_PANE);
    expect("TM_PANE in PANE does nothing", "");

    in_mode(KB_MODE_WITHIN, TMOD_NONE);
    press(KBK_TM_EXIT);
    expect("TM_EXIT from WITHIN quits copy mode first", "F20 | state | layers OFF | state");

    in_mode(KB_MODE_PANE, TMOD_NONE);
    press(KBK_TM_EXIT);
    expect("TM_EXIT from PANE", "layers OFF | state");

    in_mode(KB_MODE_PANE, TMOD_NONE);
    press(KBK_TM_DTCH);
    expect("TM_DTCH detaches, then leaves tmux mode", "C-F17 | state | layers OFF | state");
}

static void test_tree_keys(void) {
    in_mode(KB_MODE_TREE, TMOD_NONE);
    press(KBK_TT_SEL);
    expect("TT_SEL chooses and the tree closes", "tap ENT | layers PANE | state");

    in_mode(KB_MODE_TREE, TMOD_NONE);
    press(KBK_TT_KILL);
    expect("TT_KILL asks", "tap X");
    check_eq(kb.kill_pending, true, "the kill is pending");
    press(KBK_TT_KILL);
    expect("TT_KILL again answers yes", "tap X | tap Y");
    check_eq(kb.kill_pending, false, "and is done");

    in_mode(KB_MODE_TREE, TMOD_NONE);
    press(KBK_TT_KILL);
    n_calls = 0;
    check_eq(kb_key(KBK_OTHER, true), true, "another key still goes on to QMK");
    expect("any other key answers no first", "tap N");
    check_eq(kb.kill_pending, false, "and the kill is off");

    in_mode(KB_MODE_TREE, TMOD_NONE);
    press(KBK_TT_KILL);
    n_calls = 0;
    kb_key(KBK_OTHER, false);
    expect("a release does not answer", "");
    check_eq(kb.kill_pending, true, "the kill is still pending");
}

static void test_os_swap(void) {
    reset(KB_OS_MAC);
    press(KBK_OS_SWAP);
    expect("follow to force mac: already on mac", "state");
    check_eq(calls[0].report[10], KB_OS_MAC | KB_OS_OVERRIDE, "force mac: the override bit");
    n_calls = 0;
    press(KBK_OS_SWAP);
    expect("force mac to force linux", "base LINUX | state");
    check_eq(calls[1].report[10], KB_OS_LINUX | KB_OS_OVERRIDE, "force linux: on linux, overriding");
    n_calls = 0;
    press(KBK_OS_SWAP);
    expect("back to follow, with no OS reported: layer stays", "state");
    check_eq(calls[0].report[10], KB_OS_LINUX, "follow: no override bit");
}

// WITHIN on a pane the host describes, with nothing recorded yet.
static void in_within(uint8_t program, uint8_t tmux_bits, uint8_t transcript) {
    reset(KB_OS_MAC);
    kb_context_t c = steady();
    c.program      = program;
    c.tmux_bits    = KB_TMUX_PRESENT | KB_TMUX_FOCUSED | tmux_bits;
    c.transcript   = transcript;
    receive(&c);
    kb.mode = KB_MODE_WITHIN;
    n_calls = 0;
}

static void test_within_entry(void) {
    in_mode(KB_MODE_PANE, TMOD_NONE);
    press(KBK_TM_WITHIN);
    expect("TM_WITHIN on a shell opens copy mode", "layers WITHIN | state | F19 | state");

    in_mode(KB_MODE_PANE, TMOD_NONE);
    kb.ctx.program = KB_PROGRAM_CLAUDE;
    press(KBK_TM_WITHIN);
    expect("TM_WITHIN on Claude asks the host to open the viewer", "layers WITHIN | state | state");
    check_eq(calls[2].report[5], KB_INTENT_WITHIN_OPEN, "the intent is WITHIN_OPEN");

    in_mode(KB_MODE_PANE, TMOD_NONE);
    kb.ctx.program = KB_PROGRAM_HUNK;
    press(KBK_TM_WITHIN);
    expect("TM_WITHIN on hunk needs nothing opened", "layers WITHIN | state");

    in_within(KB_PROGRAM_CLAUDE, 0, KB_TRANSCRIPT_OPEN);
    press(KBK_TM_WITHIN);
    expect("TM_WITHIN in the viewer asks for the whole conversation", "F20 | state | layers WITHIN | state | state");
    check_eq(calls[4].report[5], KB_INTENT_WITHIN_DEEP, "the intent is WITHIN_DEEP");

    in_within(KB_PROGRAM_CLAUDE, KB_TMUX_COPY_MODE, KB_TRANSCRIPT_OPEN);
    press(KBK_TM_WITHIN);
    expect("TM_WITHIN in copy mode over Claude backs out to the viewer", "F20 | state");
    check_eq(kb.mode, KB_MODE_WITHIN, "still WITHIN");

    in_within(KB_PROGRAM_SHELL, KB_TMUX_COPY_MODE, KB_TRANSCRIPT_UNKNOWN);
    press(KBK_TM_WITHIN);
    expect("TM_WITHIN in copy mode over a shell leaves", "F20 | state | layers PANE | state");

    // A dead host reads as copy mode, and its last word on the program cannot
    // be trusted to have a viewer underneath.
    in_within(KB_PROGRAM_CLAUDE, 0, KB_TRANSCRIPT_OPEN);
    now_ms += KB_HOST_TIMEOUT_MS;
    press(KBK_TM_WITHIN);
    expect("TM_WITHIN with the host gone leaves, even over Claude", "F20 | state | layers PANE | state");

    in_within(KB_PROGRAM_HUNK, 0, KB_TRANSCRIPT_UNKNOWN);
    press(KBK_TM_WITHIN);
    expect("TM_WITHIN on hunk has nothing to unwind", "");
}

static void test_within_keys(void) {
    in_within(KB_PROGRAM_SHELL, KB_TMUX_COPY_MODE, KB_TRANSCRIPT_UNKNOWN);
    press(KBK_TC_UP);
    expect("TC_UP in copy mode", "tap16 UP");

    in_within(KB_PROGRAM_SHELL, KB_TMUX_COPY_MODE, KB_TRANSCRIPT_UNKNOWN);
    kb.mod = TMOD_LINE;
    press(KBK_TC_UP);
    expect("TC_UP with LINE in copy mode", "tap16 PGUP");

    in_within(KB_PROGRAM_HUNK, 0, KB_TRANSCRIPT_UNKNOWN);
    press(KBK_TC_LEFT);
    expect("TC_LEFT in hunk", "tap16 LBRC");

    in_within(KB_PROGRAM_CLAUDE, 0, KB_TRANSCRIPT_UNKNOWN);
    press(KBK_TC_LEAVE);
    expect("TC_LEAVE at a live Claude prompt sends nothing", "");

    in_within(KB_PROGRAM_CLAUDE, 0, KB_TRANSCRIPT_OPEN);
    press(KBK_TC_BKGD);
    expect("TC_BKGD in the viewer is a table row", "C-F23 | state");

    in_within(KB_PROGRAM_SHELL, KB_TMUX_COPY_MODE, KB_TRANSCRIPT_UNKNOWN);
    press(KBK_TC_COPY);
    expect("TC_COPY in copy mode copies, then PANE", "tap16 ENT | layers PANE | state");

    in_within(KB_PROGRAM_CLAUDE, 0, KB_TRANSCRIPT_OPEN);
    press(KBK_TC_COPY);
    expect("TC_COPY in the viewer does nothing", "");
    check_eq(kb.mode, KB_MODE_WITHIN, "and stays in WITHIN");

    in_within(KB_PROGRAM_SHELL, KB_TMUX_COPY_MODE, KB_TRANSCRIPT_UNKNOWN);
    press(KBK_TC_PSTE);
    expect("TC_PSTE in copy mode quits, pastes, then leaves tmux mode", "F20 | state | C-F22 | state | layers OFF | state");

    // A new intent forgets the last one's answer.
    in_mode(KB_MODE_PANE, TMOD_NONE);
    kb.ctx.program    = KB_PROGRAM_CLAUDE;
    kb.intent_acked   = KB_INTENT_FAILED;
    kb.intent_failed  = true;
    press(KBK_TM_WITHIN);
    check_eq(kb.intent_acked, KB_INTENT_NONE, "intent: the old answer forgotten");
    check_eq(kb.intent_failed, false, "intent: the failed mark cleared");
}

int main(void) {
    test_hello();
    test_rejected_report();
    test_alive();
    test_ack();
    test_follow_os();
    test_forced_os();
    test_adopt_pane();
    test_intent_result();
    test_state_fields();
    test_set_mode();
    test_pane_arrows();
    test_pane_keys();
    test_held_thumbs();
    test_releases();
    test_window_keys();
    test_mode_keys();
    test_tree_keys();
    test_os_swap();
    test_within_entry();
    test_within_keys();

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("kb_machine: all checks passed\n");
    return 0;
}

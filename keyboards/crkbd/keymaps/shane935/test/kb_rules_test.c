// Unit tests for kb_rules.c. Run with test/run.sh.

#include "../kb_rules.h"

#include <stdio.h>
#include <string.h>

#include "modifiers.h"
#include "quantum_keycodes.h"

static int failures;

static void check_eq(unsigned got, unsigned want, const char *what) {
    if (got != want) {
        printf("FAIL %s: got 0x%04X, want 0x%04X\n", what, got, want);
        failures++;
    }
}

static void check(bool ok, const char *what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        failures++;
    }
}

// ------------------------------------------------------------------- os

static void test_os_cycle(void) {
    check_eq(kb_os_next(KB_OS_FOLLOW), KB_OS_FORCE_MAC, "follow then forces mac");
    check_eq(kb_os_next(KB_OS_FORCE_MAC), KB_OS_FORCE_LINUX, "mac then forces linux");
    check_eq(kb_os_next(KB_OS_FORCE_LINUX), KB_OS_FOLLOW, "linux then follows again");

    // The behaviour the key exists for: whatever you are in, three taps hands
    // the OS back to the host.
    for (uint8_t start = KB_OS_FOLLOW; start <= KB_OS_FORCE_LINUX; start++) {
        check_eq(kb_os_next(kb_os_next(kb_os_next(start))), start, "three taps returns where it started");
    }
    check_eq(kb_os_next(kb_os_next(kb_os_next(KB_OS_FOLLOW))), KB_OS_FOLLOW, "three taps from follow is follow");
}

static void test_os_wanted(void) {
    check_eq(kb_os_wanted(KB_OS_FOLLOW, KB_OS_LINUX), KB_OS_LINUX, "following a linux host");
    check_eq(kb_os_wanted(KB_OS_FOLLOW, KB_OS_MAC), KB_OS_MAC, "following a mac host");
    // The cell that matters: no answer yet is not an answer, and the layer has
    // to stay where it is rather than defaulting to either OS.
    check_eq(kb_os_wanted(KB_OS_FOLLOW, KB_OS_UNKNOWN), KB_OS_UNKNOWN, "following a host that has not said");
    check_eq(kb_os_wanted(KB_OS_FOLLOW, 99), KB_OS_UNKNOWN, "following a host talking nonsense");

    // A force ignores the report entirely, including a contradicting one.
    check_eq(kb_os_wanted(KB_OS_FORCE_MAC, KB_OS_LINUX), KB_OS_MAC, "forced mac over a linux host");
    check_eq(kb_os_wanted(KB_OS_FORCE_LINUX, KB_OS_MAC), KB_OS_LINUX, "forced linux over a mac host");
    check_eq(kb_os_wanted(KB_OS_FORCE_MAC, KB_OS_UNKNOWN), KB_OS_MAC, "forced mac with no host at all");
}

// --------------------------------------------------------------- liveness

static void test_host_alive(void) {
    check(!kb_host_alive(0, 0), "nothing has landed at boot");
    check(!kb_host_alive(0, 500), "still nothing 500 ms in");
    // Without the zero rule this is the bug: 500 is inside the timeout of a
    // last_ms that only looks like a timestamp.
    check(!kb_host_alive(0, 1499), "a zero last_ms is never alive");

    check(kb_host_alive(1000, 1000), "a report that just landed");
    check(kb_host_alive(1000, 2499), "1499 ms later is still alive");
    check(!kb_host_alive(1000, 2500), "exactly the timeout is dead");
    check(!kb_host_alive(1000, 9000), "long past the timeout is dead");

    // The timer wraps about every 49 days, and unsigned subtraction is what
    // keeps a report from just before the wrap readable just after it.
    check(kb_host_alive(0xFFFFFF00, 0xFFFFFF00 + 100), "alive across the wrap");
    check(!kb_host_alive(0xFFFFFF00, 0xFFFFFF00 + 2000), "dead across the wrap");
}

// ---------------------------------------------------------- contradiction

static kb_context_t ctx_of(uint8_t bits, uint8_t intent_status) {
    kb_context_t c;
    memset(&c, 0, sizeof(c));
    c.tmux_bits     = bits;
    c.intent_status = intent_status;
    return c;
}

static void test_adopt_pane(void) {
    kb_context_t clear = ctx_of(KB_TMUX_PRESENT, KB_INTENT_NONE);
    check(kb_adopt_pane(&clear, true), "no mode open while the keyboard thinks one is");
    check(!kb_adopt_pane(&clear, false), "nothing to adopt when the keyboard claims no mode");

    kb_context_t copying = ctx_of(KB_TMUX_PRESENT | KB_TMUX_COPY_MODE, KB_INTENT_NONE);
    check(!kb_adopt_pane(&copying, true), "copy mode open agrees with the keyboard");

    kb_context_t tree = ctx_of(KB_TMUX_PRESENT | KB_TMUX_OTHER_MODE, KB_INTENT_NONE);
    check(!kb_adopt_pane(&tree, true), "a tree open agrees with the keyboard");

    // Not a contradiction: a report from outside tmux has no mode
    // open because there is nothing to have open, and reading that as a
    // contradiction would knock you out of copy mode on every window switch.
    kb_context_t no_tmux = ctx_of(0, KB_INTENT_NONE);
    check(!kb_adopt_pane(&no_tmux, true), "no tmux is not a contradiction");

    // An intent in flight explains the mismatch, so it is not the world moving.
    kb_context_t pending = ctx_of(KB_TMUX_PRESENT, KB_INTENT_PENDING);
    check(!kb_adopt_pane(&pending, true), "a pending intent explains it");
    kb_context_t failed = ctx_of(KB_TMUX_PRESENT, KB_INTENT_FAILED);
    check(!kb_adopt_pane(&failed, true), "a failed intent is handled on its own path");
}

static void test_intent_settled(void) {
    kb_context_t c;
    memset(&c, 0, sizeof(c));
    c.intent_nonce = 7;

    c.intent_status = KB_INTENT_DONE;
    check(kb_intent_settled(&c, 7), "done for this nonce");
    // The point of the nonce: a status for an older intent must not be read as
    // an answer to the one just sent.
    check(!kb_intent_settled(&c, 8), "done for an older nonce is not an answer");

    c.intent_status = KB_INTENT_FAILED;
    check(kb_intent_settled(&c, 7), "failed for this nonce");
    c.intent_status = KB_INTENT_PENDING;
    check(!kb_intent_settled(&c, 7), "pending is not settled");
    c.intent_status = KB_INTENT_NONE;
    check(!kb_intent_settled(&c, 7), "no intent is not settled");
}

// -------------------------------------------------------------- tmux rows

static void test_tmux_rows(void) {
    // Every row of the table, which is also what pins QK_LCTL >> 8 == MOD_LCTL
    // rather than leaving it to a reading of two headers.
    for (uint8_t f = KC_F13; f <= KC_F24; f++) {
        char what[64];
        snprintf(what, sizeof(what), "F%d bare is a row", 13 + (f - KC_F13));
        check(kb_is_tmux_row(f), what);
        check_eq(kb_tmux_row_key(f), f, "bare row keeps its key");
        check_eq(kb_tmux_row_mods(f), 0, "bare row carries no mods");

        check(kb_is_tmux_row(S(f)), "shifted is a row");
        check_eq(kb_tmux_row_key(S(f)), f, "shifted keeps its key");
        check_eq(kb_tmux_row_mods(S(f)), MOD_LSFT, "shifted gives MOD_LSFT");

        check(kb_is_tmux_row(C(f)), "ctrl is a row");
        check_eq(kb_tmux_row_key(C(f)), f, "ctrl keeps its key");
        check_eq(kb_tmux_row_mods(C(f)), MOD_LCTL, "ctrl gives MOD_LCTL");

        check(kb_is_tmux_row(A(f)), "alt is a row");
        check_eq(kb_tmux_row_key(A(f)), f, "alt keeps its key");
        check_eq(kb_tmux_row_mods(A(f)), MOD_LALT, "alt gives MOD_LALT");
    }

    // The keys the resolver actually emits alongside them, none of which may be
    // mistaken for a row and sent as a function key.
    const uint16_t keystrokes[] = {KC_UP, KC_DOWN, KC_LEFT, KC_RGHT, KC_PGUP, KC_PGDN, KC_B, KC_W, KC_U, KC_D, KC_N, KC_0, KC_SPC, KC_ENT, KC_ESC, KC_SLSH, KC_LBRC, KC_RBRC, KC_COMM, KC_DOT, KC_DLR, KC_LCBR, KC_RCBR, S(KC_N), S(KC_V), C(KC_O), C(KC_U), C(KC_D), C(KC_X), C(KC_B)};
    for (size_t i = 0; i < sizeof(keystrokes) / sizeof(keystrokes[0]); i++) {
        char what[64];
        snprintf(what, sizeof(what), "keycode 0x%04X is not a tmux row", keystrokes[i]);
        check(!kb_is_tmux_row(keystrokes[i]), what);
    }

    // Just outside the range on both sides.
    check(!kb_is_tmux_row(KC_F12), "F12 is not in the table");
    check(!kb_is_tmux_row(KC_F13 - 1), "the key below F13 is not in the table");
    check(!kb_is_tmux_row(KC_F24 + 1), "the key above F24 is not in the table");

    // A tap-hold whose tap happens to be a table row is not a row: sending it
    // as a function key would drop the hold entirely.
    check(!kb_is_tmux_row(LT(3, KC_F13)), "a layer-tap ending in F13 is not a row");
    check(!kb_is_tmux_row(LCTL_T(KC_F13)), "a mod-tap ending in F13 is not a row");
    // Right-hand modifiers are not in the table either; the daemon only binds
    // the left forms, so sending one would reach nothing.
    check(!kb_is_tmux_row(RCTL(KC_F13)), "a right-ctrl F13 is not a row");
}

int main(void) {
    test_os_cycle();
    test_os_wanted();
    test_host_alive();
    test_adopt_pane();
    test_intent_settled();
    test_tmux_rows();

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("kb_rules: all checks passed\n");
    return 0;
}

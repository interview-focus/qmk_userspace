// Unit tests for kb_protocol.c, the byte offsets and bit packing of
// docs/00-protocol.md. Run with test/run.sh.
//
// An offset or a bit that is wrong by one is invisible on the keyboard and
// shows up as the daemon quietly misreading every report.

#include "../kb_protocol.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check(bool ok, const char *what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        failures++;
    }
}

static void check_eq(unsigned got, unsigned want, const char *what) {
    if (got != want) {
        printf("FAIL %s: got %u, want %u\n", what, got, want);
        failures++;
    }
}

// A STATE carrying real protocol values, for the tests that are about meaning.
// Deliberately not used for the offset test: the real values collide -- KB_STATE
// and KB_OS_MAC are both 2, KB_MODE_WITHIN is 5 like the WORD modifier, and
// KB_EVENT_TMUX_KEY is 1 -- so a swapped pair of offsets would read as correct.
static kb_state_t sample_state(void) {
    kb_state_t s = {
        .type        = KB_STATE,
        .seq         = 0x11,
        .ack         = 0x22,
        .intent      = KB_INTENT_WITHIN_DEEP,
        .intent_arg  = 0x33,
        .nonce       = 0x44,
        .mode        = KB_MODE_WITHIN,
        .mod         = 6,
        .os          = KB_OS_MAC,
        .os_override = false,
        .host_alive  = true,
        .event       = KB_EVENT_TMUX_KEY,
        .event_arg   = 0x55,
    };
    return s;
}

static void test_state_offsets(void) {
    // Sentinels, not protocol values: this test is only about which byte each
    // field lands in, and every one has to differ for that to mean anything.
    // 0x2A for os keeps bit 7 clear so the override bit stays its own test.
    kb_state_t s = {
        .type        = 0xA1,
        .seq         = 0xA2,
        .ack         = 0xA3,
        .intent      = 0xA4,
        .intent_arg  = 0xA5,
        .nonce       = 0xA6,
        .mode        = 0xA7,
        .mod         = 0xA8,
        .os          = 0x2A,
        .os_override = false,
        .host_alive  = true,
        .event       = 0xA9,
        .event_arg   = 0xAA,
    };
    uint8_t buf[KB_REPORT_SIZE];
    memset(buf, 0xEE, sizeof(buf));
    kb_state_report(buf, &s);

    check_eq(buf[0], KB_MAGIC, "byte 0 magic");
    check_eq(buf[1], KB_VERSION, "byte 1 version");
    check_eq(buf[2], 0xA2, "byte 2 seq");
    check_eq(buf[3], 0xA3, "byte 3 ack");
    check_eq(buf[4], 0xA1, "byte 4 type");
    check_eq(buf[5], 0xA4, "byte 5 intent");
    check_eq(buf[6], 0xA5, "byte 6 intent arg");
    check_eq(buf[7], 0xA6, "byte 7 nonce");
    check_eq(buf[8], 0xA7, "byte 8 mode");
    check_eq(buf[9], 0xA8, "byte 9 held modifier");
    check_eq(buf[10], 0x2A, "byte 10 os");
    check_eq(buf[11], KB_FLAG_HOST_ALIVE, "byte 11 flags");
    check_eq(buf[12], 0xA9, "byte 12 event");
    check_eq(buf[13], 0xAA, "byte 13 event arg");

    // 14..31 are zero, and the 0xEE fill is what proves they are written rather
    // than left as whatever the caller's stack held.
    for (int i = 14; i < KB_REPORT_SIZE; i++) {
        char what[32];
        snprintf(what, sizeof(what), "byte %d padding", i);
        check_eq(buf[i], 0, what);
    }
}

static void test_os_override_bit(void) {
    kb_state_t s = sample_state();
    uint8_t    buf[KB_REPORT_SIZE];

    s.os          = KB_OS_LINUX;
    s.os_override = true;
    kb_state_report(buf, &s);
    check_eq(buf[10], KB_OS_LINUX | KB_OS_OVERRIDE, "override sets bit 7 and keeps the os");
    check_eq(buf[10] & 0x7F, KB_OS_LINUX, "os still readable under the override bit");

    s.os_override = false;
    kb_state_report(buf, &s);
    check_eq(buf[10], KB_OS_LINUX, "no override, no bit 7");
}

static void test_host_alive_flag(void) {
    kb_state_t s = sample_state();
    uint8_t    buf[KB_REPORT_SIZE];

    s.host_alive = false;
    kb_state_report(buf, &s);
    check_eq(buf[11], 0, "dead host clears the flag byte");
}

static void test_hello_carries_state(void) {
    // HELLO is a STATE with a different type byte, not a different layout: the
    // host applies the mode and os out of the first message like any other.
    kb_state_t s = sample_state();
    s.type       = KB_HELLO;
    uint8_t buf[KB_REPORT_SIZE];
    kb_state_report(buf, &s);
    check_eq(buf[4], KB_HELLO, "hello type");
    check_eq(buf[8], KB_MODE_WITHIN, "hello still carries the mode");
}

// A CONTEXT with every field distinct, for the same reason.
static void fill_context(uint8_t *buf, const char *label) {
    memset(buf, 0, KB_REPORT_SIZE);
    buf[0]  = KB_MAGIC;
    buf[1]  = KB_VERSION;
    buf[2]  = 0x66;
    buf[3]  = 0x77;
    buf[4]  = KB_CONTEXT;
    buf[5]  = KB_OS_LINUX;
    buf[6]  = KB_PROGRAM_CLAUDE;
    buf[7]  = KB_TMUX_PRESENT | KB_TMUX_FOCUSED;
    buf[8]  = KB_TRANSCRIPT_OPEN;
    buf[9]  = KB_PHASE_RUNNING;
    buf[10] = 3;
    buf[11] = 0x88;
    buf[12] = KB_INTENT_PENDING;
    buf[13] = 4;
    buf[14] = 5;
    // The wire field is 16 bytes and is not terminated when it is full.
    size_t n = strlen(label);
    memcpy(&buf[15], label, n > 16 ? 16 : n);
}

static void test_context_fields(void) {
    uint8_t      buf[KB_REPORT_SIZE];
    kb_context_t c;
    memset(&c, 0, sizeof(c));
    fill_context(buf, "build");

    check(kb_context_parse(buf, KB_REPORT_SIZE, &c), "a good context parses");
    check_eq(c.seq, 0x66, "context seq");
    check_eq(c.ack, 0x77, "context ack");
    check_eq(c.os, KB_OS_LINUX, "context os");
    check_eq(c.program, KB_PROGRAM_CLAUDE, "context program");
    check_eq(c.tmux_bits, KB_TMUX_PRESENT | KB_TMUX_FOCUSED, "context tmux bits");
    check_eq(c.transcript, KB_TRANSCRIPT_OPEN, "context transcript");
    check_eq(c.phase, KB_PHASE_RUNNING, "context phase");
    check_eq(c.perm, 3, "context permission mode");
    check_eq(c.intent_nonce, 0x88, "context intent nonce");
    check_eq(c.intent_status, KB_INTENT_PENDING, "context intent status");
    check_eq(c.window, 4, "context window index");
    check_eq(c.pane, 5, "context pane index");
    check(strcmp(c.label, "build") == 0, "context label");
}

static void test_context_label_uses_all_sixteen(void) {
    uint8_t      buf[KB_REPORT_SIZE];
    kb_context_t c;
    // Filled with non-zero, not zeroed: a zeroed struct already has a
    // terminator sitting at byte 16, so the parse would appear to write one
    // whether it does or not.
    memset(&c, 0xFF, sizeof(c));
    // Exactly 16 characters, so the wire field is full and the terminator has
    // to come from the seventeenth byte rather than from the host.
    fill_context(buf, "0123456789abcdef");

    check(kb_context_parse(buf, KB_REPORT_SIZE, &c), "a full label parses");
    check_eq((unsigned char)c.label[16], 0, "a full 16 byte label gets a terminator");
    // Only safe to read as a string once the terminator is confirmed.
    if (c.label[16] == '\0') {
        check(strcmp(c.label, "0123456789abcdef") == 0, "a 16 character label is not truncated");
    }
}

static void test_context_rejects(void) {
    uint8_t      buf[KB_REPORT_SIZE];
    kb_context_t c;
    memset(&c, 0, sizeof(c));
    c.os = KB_OS_MAC;

    fill_context(buf, "x");
    buf[0] = 0x00;
    check(!kb_context_parse(buf, KB_REPORT_SIZE, &c), "bad magic is rejected");

    fill_context(buf, "x");
    buf[1] = KB_VERSION + 1;
    check(!kb_context_parse(buf, KB_REPORT_SIZE, &c), "another version is rejected");

    fill_context(buf, "x");
    buf[4] = KB_STATE;
    check(!kb_context_parse(buf, KB_REPORT_SIZE, &c), "the keyboard's own message type is rejected");

    fill_context(buf, "x");
    check(!kb_context_parse(buf, KB_REPORT_SIZE - 1, &c), "a short report is rejected");

    // Every rejection above left the caller's context untouched, which is what
    // lets kb_receive keep using it after a bad report.
    check_eq(c.os, KB_OS_MAC, "a rejected report does not touch the context");
}

static void test_tmux_event_arg(void) {
    check_eq(kb_tmux_event_arg(1, false, false, false), 0x01, "F13 bare");
    check_eq(kb_tmux_event_arg(12, false, false, false), 0x0C, "F24 bare");
    check_eq(kb_tmux_event_arg(1, true, false, false), 0x11, "S-F13");
    check_eq(kb_tmux_event_arg(6, true, false, false), 0x16, "S-F18 next-layout");
    check_eq(kb_tmux_event_arg(5, false, true, false), 0x25, "C-F17 detach");
    check_eq(kb_tmux_event_arg(4, false, false, true), 0x44, "M-F16 swap-pane right");
    // The row survives the modifier bits, which is the thing that breaks if the
    // table ever grows past twelve rows.
    check_eq(kb_tmux_event_arg(12, true, true, true) & 0x0F, 12, "row readable under every modifier");
}

int main(void) {
    test_state_offsets();
    test_os_override_bit();
    test_host_alive_flag();
    test_hello_carries_state();
    test_context_fields();
    test_context_label_uses_all_sixteen();
    test_context_rejects();
    test_tmux_event_arg();

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("kb_protocol: all checks passed\n");
    return 0;
}

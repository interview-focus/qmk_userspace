// Unit tests for kb_machine.c. Run with test/run.sh.
//
// The kb_io_ functions are defined here to record what the machine asked for,
// in order, so each test checks the exact calls a report or a STATE produced.

#include "../kb_machine.h"
#include "../kb_rules.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check_eq(unsigned got, unsigned want, const char *what) {
    if (got != want) {
        printf("FAIL %s: got %u, want %u\n", what, got, want);
        failures++;
    }
}

// ------------------------------------------------------------------- io

enum call_kind { CALL_SEND, CALL_LAYERS, CALL_BASE };

typedef struct {
    enum call_kind kind;
    uint8_t        arg;
    uint8_t        report[KB_REPORT_SIZE];
} call_t;

static call_t   calls[16];
static int      n_calls;
static uint32_t now_ms;

static void record(enum call_kind kind, uint8_t arg, const uint8_t *report) {
    if (n_calls == (int)(sizeof calls / sizeof calls[0])) {
        printf("FAIL more calls than the recorder holds\n");
        failures++;
        return;
    }
    calls[n_calls].kind = kind;
    calls[n_calls].arg  = arg;
    if (report) {
        memcpy(calls[n_calls].report, report, KB_REPORT_SIZE);
    }
    n_calls++;
}

uint32_t kb_io_now_ms(void) {
    return now_ms;
}

void kb_io_send(const uint8_t report[KB_REPORT_SIZE]) {
    record(CALL_SEND, 0, report);
}

void kb_io_set_mode_layers(uint8_t mode) {
    record(CALL_LAYERS, mode, NULL);
}

void kb_io_set_base(uint8_t os) {
    record(CALL_BASE, os, NULL);
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

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("kb_machine: all checks passed\n");
    return 0;
}

#include "kb_machine.h"

#include "kb_rules.h"

kb_machine_t kb;

void kb_machine_init(uint8_t base_os) {
    kb = (kb_machine_t){
        .mode      = KB_MODE_OFF,
        .mod       = TMOD_NONE,
        .os_source = KB_OS_FOLLOW,
        .base_os   = base_os,
        .ctx       = {.seq = KB_SEQ_NONE},
    };
}

bool kb_alive(void) {
    return kb_host_alive(kb.ctx_last_ms, kb_io_now_ms());
}

within_target_t kb_within_now(void) {
    return within_target(&kb.ctx, kb_alive());
}

// The layer is persisted, as it has always been, so an unplugged keyboard keeps
// the last one it was on -- the override itself is not, so a replug comes back
// up following the host.
void kb_apply_os(void) {
    uint8_t want = kb_os_wanted(kb.os_source, kb.ctx.os);
    if (want == KB_OS_UNKNOWN || want == kb.base_os) {
        return;
    }
    kb.base_os = want;
    kb_io_set_base(want);
}

void kb_set_mode(uint8_t mode) {
    kb_io_set_mode_layers(mode);
    kb.mode = mode;
    // The thumb modifiers belong to the mode they were pressed in, so WITHIN's
    // WORD can never survive into PANE.
    kb.mod = TMOD_NONE;
    kb_send_state(0, 0);
}

static void send_state_msg(uint8_t type, uint8_t intent, uint8_t arg) {
    // The nonce is what makes an intent one-shot: the host actuates once per
    // nonce, so a resend of the same intent is a resend and not a second go.
    if (intent) {
        kb.nonce++;
    }
    kb_state_t state = {
        .type        = type,
        .seq         = ++kb.my_seq,
        .ack         = kb.ctx.seq,
        .intent      = intent,
        .intent_arg  = arg,
        .nonce       = kb.nonce,
        .mode        = kb.mode,
        .mod         = kb.mod,
        .os          = kb.base_os,
        .os_override = kb.os_source != KB_OS_FOLLOW,
        .host_alive  = kb_alive(),
        .event       = kb.event,
        .event_arg   = kb.event_arg,
    };
    uint8_t report[KB_REPORT_SIZE];
    kb_state_report(report, &state);
    kb_io_send(report);
    kb.event     = KB_EVENT_NONE;
    kb.event_arg = 0;
}

void kb_send_state(uint8_t intent, uint8_t arg) {
    send_state_msg(KB_STATE, intent, arg);
}

void kb_hello(void) {
    send_state_msg(KB_HELLO, 0, 0);
}

void kb_receive(const uint8_t *data, uint8_t length) {
    if (!kb_context_parse(data, length, &kb.ctx)) {
        return;
    }
    // kb_alive reads zero as "no CONTEXT yet", and the timer can legitimately
    // return zero, so that one value is never stored.
    kb.ctx_last_ms = kb_io_now_ms();
    if (kb.ctx_last_ms == 0) {
        kb.ctx_last_ms = 1;
    }

    kb_apply_os();

    // The safety net, not a normal path: the world moved without the keyboard
    // -- a pane closed, a program exited -- so the host is right and the
    // keyboard adopts PANE. kb_set_mode sends the corrected state itself.
    bool claims_mode = kb.mode == KB_MODE_TREE || (kb.mode == KB_MODE_WITHIN && kb_within_now() == WT_COPY);
    if (kb_adopt_pane(&kb.ctx, claims_mode)) {
        kb_set_mode(KB_MODE_PANE);
        return;
    }

    // The intent is one-shot, so this is the whole of the keyboard's part in it.
    // Edge triggered on the status, not level: the host keeps reporting the same
    // answer every 500 ms until the next intent, so acting on it each time would
    // hold the OLED mark on for as long as the daemon kept talking. Neither
    // intent moves the mode: both are entered from WITHIN and both succeed by
    // changing which target resolves, so a failure needs nothing undone and the
    // mark is the whole of the keyboard's response.
    if (kb_intent_settled(&kb.ctx, kb.nonce) && kb.ctx.intent_status != kb.intent_acked) {
        kb.intent_acked   = kb.ctx.intent_status;
        kb.intent_failed  = kb.ctx.intent_status == KB_INTENT_FAILED;
        kb.intent_mark_ms = kb_io_now_ms();
    }

    // A CONTEXT whose ack is not the current seq answered an older STATE, so
    // say the current one again. The host acks the latest report it has seen
    // rather than the one it is replying to, so a STATE that crossed a CONTEXT
    // in flight costs one extra round trip and then settles.
    if (kb.ctx.ack != kb.my_seq) {
        kb_send_state(0, 0);
    }
}

#include "kb_machine.h"

#include "kb_rules.h"
#include "modifiers.h"
#include "quantum_keycodes.h"

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
    kb.mode            = mode;
    kb.mode_entered_ms = kb_io_now_ms();
    kb.mode_seen_open  = false;
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
    kb_io_send(report, &state);
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
    // Only the mode's own bit counts as seen, so a tree still showing on the
    // way from TREE to WITHIN is not copy mode.
    bool claims_mode = kb.mode == KB_MODE_TREE || (kb.mode == KB_MODE_WITHIN && kb_within_now() == WT_COPY);
    uint8_t own_bit  = kb.mode == KB_MODE_TREE ? KB_TMUX_OTHER_MODE : kb.mode == KB_MODE_WITHIN ? KB_TMUX_COPY_MODE : 0;
    if (kb.ctx.tmux_bits & own_bit) {
        kb.mode_seen_open = true;
    }
    bool host_had_time = kb.mode_seen_open || (uint32_t)(kb.ctx_last_ms - kb.mode_entered_ms) >= KB_HOST_TIMEOUT_MS;
    if (host_had_time && kb_adopt_pane(&kb.ctx, claims_mode)) {
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

// Every tmux action is one row of the key table in keyboard-daemon's
// src/keytable.ts: a single root-table key, which tmux runs the instant it
// arrives. The commands
// themselves -- and so the directory a new pane opens in, and every other flag
// -- live in the generated tmux.conf, not here. mods names the row, so S-F19 in
// the table is tmux_fkey(KC_F19, MOD_LSFT) here.
static void tmux_fkey(uint8_t fkey, uint8_t mods) {
    kb_io_tap_row(fkey, mods);
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
            kb_io_tap_code16(pair[i]);
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

bool kb_key(kb_key_t key, bool pressed) {
    // The kill prompt is tmux's, not the keyboard's: while it is up it swallows
    // the next key whatever that key was meant for. So every key except the one
    // that answers it says no first and then does its own job, and the prompt
    // can never outlive the press that raised it.
    if (pressed && kb.kill_pending && key != KBK_TT_KILL) {
        kb_io_tap_code(KC_N);
        kb.kill_pending = false;
    }

    // Every thumb modifier is held, so these are the only keys that have
    // anything to do on the release.
    switch (key) {
        case KBK_TP_RSZE:
            tmux_set_mod(pressed ? TMOD_RESIZE : TMOD_NONE);
            return false;
        case KBK_TP_SPLT:
            tmux_set_mod(pressed ? TMOD_SPLIT : TMOD_NONE);
            return false;
        case KBK_TP_MOVE:
        case KBK_TW_MOVE:
            tmux_set_mod(pressed ? TMOD_MOVE : TMOD_NONE);
            return false;
        // WITHIN's two are held like PANE's: one unit per thumb, and nothing
        // survives letting go.
        case KBK_TC_WORD:
            tmux_set_mod(pressed ? TMOD_WORD : TMOD_NONE);
            return false;
        case KBK_TC_LINE:
            tmux_set_mod(pressed ? TMOD_LINE : TMOD_NONE);
            return false;
        default:
            break;
    }

    if (!pressed) {
        return true;
    }

    switch (key) {
        // Follow the host, force Mac, force Linux, follow the host again. The
        // first tap off Follow may not move the layer at all -- it is already
        // where the host put it -- so the OLED says LOCK to show what changed.
        case KBK_OS_SWAP:
            kb.os_source = kb_os_next(kb.os_source);
            kb_apply_os();
            kb_send_state(0, 0);
            return false;

        // Tapping the current mode's own key quits TREE or WITHIN and falls back
        // to PANE; in PANE and WINDOW there is nothing open to quit.
        case KBK_TMUX_ON:
            kb_set_mode(KB_MODE_PANE);
            return false;
        case KBK_TM_TREE:
            tmux_switch_mode(kb.mode == KB_MODE_TREE ? KB_MODE_PANE : KB_MODE_TREE);
            return false;
        case KBK_TM_WIN:
            if (kb.mode != KB_MODE_WINDOW) {
                tmux_switch_mode(KB_MODE_WINDOW);
            }
            return false;
        case KBK_TM_PANE:
            if (kb.mode != KB_MODE_PANE) {
                tmux_switch_mode(KB_MODE_PANE);
            }
            return false;
        // F goes deeper while there is deeper to go and comes back out when there
        // is not: on a Claude pane it opens the viewer, then asks for the whole
        // conversation in copy mode, then leaves. In copy mode or hunk there is
        // nothing further in, so it leaves straight away -- which is also what
        // stops it closing and reopening copy mode and losing the cursor.
        case KBK_TM_WITHIN:
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
        case KBK_TM_EXIT:
            // Leaving a tree or copy mode open would send the next thing typed
            // into it instead of the shell.
            tmux_quit_mode();
            kb_set_mode(KB_MODE_OFF);
            return false;
        case KBK_TM_DTCH:
            tmux_fkey(KC_F17, MOD_LCTL);
            kb_set_mode(KB_MODE_OFF);
            return false;

        case KBK_TP_UP:
            tmux_pane_arrow(KC_F15);
            return false;
        case KBK_TP_DOWN:
            tmux_pane_arrow(KC_F14);
            return false;
        case KBK_TP_LEFT:
            tmux_pane_arrow(KC_F13);
            return false;
        case KBK_TP_RGHT:
            tmux_pane_arrow(KC_F16);
            return false;
        case KBK_TP_LAST:
            tmux_fkey(KC_F17, 0);
            return false;
        case KBK_TP_ZOOM:
            tmux_fkey(KC_F18, 0);
            return false;
        case KBK_TP_LYT:
            tmux_fkey(KC_F18, MOD_LSFT);
            return false;

        // Up and down are sessions, which MOVE has nothing to say about, so
        // they are dead while it is held.
        case KBK_TW_UP:
            if (kb.mod == TMOD_NONE) {
                tmux_fkey(KC_F19, MOD_LSFT);
            }
            return false;
        case KBK_TW_DOWN:
            if (kb.mod == TMOD_NONE) {
                tmux_fkey(KC_F20, MOD_LSFT);
            }
            return false;
        case KBK_TW_LEFT:
            tmux_fkey(KC_F22, kb.mod == TMOD_MOVE ? MOD_LSFT : 0);
            return false;
        case KBK_TW_RGHT:
            tmux_fkey(KC_F23, kb.mod == TMOD_MOVE ? MOD_LSFT : 0);
            return false;
        case KBK_TW_LAST:
            tmux_fkey(KC_F21, MOD_LSFT);
            return false;

        case KBK_TT_SEL:
            // The tree's own template decides what choosing an item means, so
            // there is nothing to know here beyond "the tree is now closed".
            kb_io_tap_code(KC_ENT);
            kb_set_mode(KB_MODE_PANE);
            return false;
        // A bare x, because the tree is reading keys itself rather than through
        // the prefix. It kills whatever is highlighted, session or window, and
        // the tree stays open afterwards.
        case KBK_TT_KILL:
            if (kb.kill_pending) {
                kb_io_tap_code(KC_Y);
            } else {
                kb_io_tap_code(KC_X);
            }
            kb.kill_pending = !kb.kill_pending;
            return false;

        // Every one of these is the same two lines: ask the resolver what this
        // key means in whatever the host says is in the pane, and send that. The
        // keymap deliberately holds no opinion -- the tables are in
        // tmux_context.c and the state behind them belongs to the host.
        case KBK_TC_UP:
            within_send(WK_UP);
            return false;
        case KBK_TC_DOWN:
            within_send(WK_DOWN);
            return false;
        case KBK_TC_LEFT:
            within_send(WK_LEFT);
            return false;
        case KBK_TC_RGHT:
            within_send(WK_RIGHT);
            return false;
        case KBK_TC_SRCH:
            within_send(WK_SEARCH);
            return false;
        case KBK_TC_NEXT:
            within_send(WK_NEXT);
            return false;
        case KBK_TC_PREV:
            within_send(WK_PREV);
            return false;
        case KBK_TC_BKGD:
            within_send(WK_BKGD);
            return false;
        case KBK_TC_MARK:
            within_send(WK_MARK);
            return false;
        case KBK_TC_SEL:
            within_send(WK_SELECT);
            return false;
        // Leaves what WITHIN opened, without leaving WITHIN: closing Claude's
        // viewer lands back on its prompt, which is still a Claude pane, and the
        // next report says so.
        case KBK_TC_LEAVE:
            within_send(WK_LEAVE);
            return false;
        // The two that end the mode as well as doing something. Both are copy
        // mode only, so the mode only moves if the resolver actually sent
        // something -- in the transcript or at a Claude prompt they are dead and
        // the keyboard stays where it is.
        case KBK_TC_COPY:
            if (kb_within_now() == WT_COPY) {
                within_send(WK_COPY);
                kb_set_mode(KB_MODE_PANE);
            }
            return false;
        case KBK_TC_PSTE:
            if (kb_within_now() == WT_COPY) {
                within_send(WK_PASTE);
                kb_set_mode(KB_MODE_OFF);
            }
            return false;
        default:
            break;
    }

    return true;
}

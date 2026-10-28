// The wire format shared with keyboard-daemon, and nothing else.
//
// This file knows no QMK and no keymap: not a layer number, not a keycode, not
// a TMOD_. Everything it is handed is already in the protocol's own values, so
// it compiles natively and test/run.sh exercises it without a keyboard. The
// translation from the keymap's enums into these values is the keymap's job,
// which keeps the byte offsets in one place and the keyboard's state in
// another.

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Every report in both directions is exactly this long, zero padded. A protocol
// size, not QMK's: RAW_EPSIZE is also 32, but it lives in a tmk_core header
// keymaps do not get and is 8 on a vusb board.
#define KB_REPORT_SIZE 32

#define KB_MAGIC 0xA5
#define KB_VERSION 2
#define KB_HELLO 0x01
#define KB_STATE 0x02
#define KB_CONTEXT 0x81
// ack, when no report has been seen from the peer yet.
#define KB_SEQ_NONE 0xFF
// The host sends a CONTEXT every 500 ms whether anything changed or not, so
// this is three missed reports.
#define KB_HOST_TIMEOUT_MS 1500

// STATE byte 8. The protocol numbers the modes itself and knows nothing of the
// keyboard's layer stack.
enum kb_mode_byte { KB_MODE_OFF, KB_MODE_TMUX, KB_MODE_TREE, KB_MODE_WINDOW, KB_MODE_PANE, KB_MODE_WITHIN };
// STATE byte 10 and CONTEXT byte 5. On STATE, bit 7 says the keyboard is
// overruling the host rather than following it.
enum kb_os_byte { KB_OS_UNKNOWN, KB_OS_LINUX, KB_OS_MAC };
#define KB_OS_OVERRIDE 0x80
// STATE byte 11.
#define KB_FLAG_HOST_ALIVE 0x01
// STATE byte 12. The host never actuates this -- the keyboard already has -- it
// only schedules its observation around it.
enum kb_event_byte { KB_EVENT_NONE, KB_EVENT_TMUX_KEY };

// CONTEXT byte 6.
enum kb_program_byte { KB_PROGRAM_UNKNOWN, KB_PROGRAM_SHELL, KB_PROGRAM_CLAUDE, KB_PROGRAM_HUNK, KB_PROGRAM_OTHER };
// CONTEXT byte 7.
#define KB_TMUX_PRESENT 0x01
#define KB_TMUX_FOCUSED 0x02
#define KB_TMUX_COPY_MODE 0x04
#define KB_TMUX_OTHER_MODE 0x08
#define KB_TMUX_ZOOMED 0x10
// CONTEXT byte 8.
enum kb_transcript_byte { KB_TRANSCRIPT_UNKNOWN, KB_TRANSCRIPT_CLOSED, KB_TRANSCRIPT_OPEN };
// CONTEXT byte 9.
enum kb_phase_byte { KB_PHASE_NONE, KB_PHASE_IDLE, KB_PHASE_RUNNING, KB_PHASE_WAITING };
// CONTEXT byte 10.
enum kb_perm_byte { KB_PERM_UNKNOWN, KB_PERM_DEFAULT, KB_PERM_PLAN, KB_PERM_ACCEPT_EDITS, KB_PERM_AUTO, KB_PERM_BYPASS, KB_PERM_DONT_ASK };
// CONTEXT byte 12.
enum kb_intent_status { KB_INTENT_NONE, KB_INTENT_PENDING, KB_INTENT_DONE, KB_INTENT_FAILED };

// Intent ids. 0x10-0x14 and 0x20-0x51 are reserved: do not assign them.
#define KB_INTENT_WITHIN_DEEP 0x15
#define KB_INTENT_WITHIN_OPEN 0x16

// Everything STATE carries that the keyboard has to supply.
typedef struct {
    uint8_t type;
    uint8_t seq;
    uint8_t ack;
    uint8_t intent;
    uint8_t intent_arg;
    uint8_t nonce;
    uint8_t mode;
    uint8_t mod;
    uint8_t os;
    bool    os_override;
    bool    host_alive;
    uint8_t event;
    uint8_t event_arg;
} kb_state_t;

// Everything a CONTEXT reports. The host owns all of it.
typedef struct {
    uint8_t seq;
    uint8_t ack;
    uint8_t os;
    uint8_t program;
    uint8_t tmux_bits;
    uint8_t transcript;
    uint8_t phase;
    uint8_t perm;
    uint8_t intent_nonce;
    uint8_t intent_status;
    uint8_t window;
    uint8_t pane;
    // 16 bytes on the wire, NUL terminated here so it can be printed.
    char label[17];
} kb_context_t;

// Writes exactly KB_REPORT_SIZE bytes, zero padded.
void kb_state_report(uint8_t *buf, const kb_state_t *in);

// False when the report is not a CONTEXT this firmware understands, in which
// case out is left alone. A caller that ignores the result cannot tell a stale
// context from a rejected one, so don't.
bool kb_context_parse(const uint8_t *data, uint8_t length, kb_context_t *out);

// STATE byte 13 for KB_EVENT_TMUX_KEY: which row of the tmux key table went
// out. row is 1 for F13 through 12 for F24.
uint8_t kb_tmux_event_arg(uint8_t row, bool shift, bool ctrl, bool alt);

// Records what the keyboard sends, as the contract the daemon verifies itself
// against, in two parts:
//
//   expected.json, *.bin           single reports from kb_state_report, chosen
//                                  by hand to cover each field's range
//   interactions.json,             whole conversations replayed through
//   interactions/*.bin             kb_machine.c, recording what it sent
//
// The bytes are whatever the encoder produces; the JSON holds the values that
// went in.
//
//   test/run.sh record
//
// Deliberate, never automatic: the committed files are the agreement, so a
// change to them has to appear in a diff and be explained. A plain run checks
// they still match the encoder.

#include "../kb_machine.h"
#include "../kb_protocol.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "modifiers.h"
#include "quantum_keycodes.h"

// Which tmux key a STATE with KB_EVENT_TMUX_KEY says went out. Passed to
// kb_tmux_event_arg rather than worked out here.
typedef struct {
    uint8_t row;
    bool    shift, ctrl, alt;
} tmux_key_t;

typedef struct {
    const char *file;
    const char *name;
    const char *why;
    kb_state_t  state;
    tmux_key_t  key;
} contract_case_t;

static const contract_case_t cases[] = {
    {
        .file  = "hello",
        .name  = "hello",
        .why   = "The first message after connecting: nothing seen from the host yet, so ack is none and the host is not yet alive.",
        .state = { .type = KB_HELLO, .seq = 0, .ack = KB_SEQ_NONE, .os = KB_OS_MAC },
    },
    {
        .file  = "typical",
        .name  = "typical",
        .why   = "PANE mode with RESIZE held, on mac, the host alive and acknowledged.",
        .state = { .type = KB_STATE, .seq = 7, .ack = 42, .nonce = 3, .mode = KB_MODE_PANE, .mod = 1, .os = KB_OS_MAC, .host_alive = true },
    },
    {
        .file  = "maximum",
        .name  = "every field at its maximum",
        .why   = "Each field at the top of its range, so a field read as too narrow fails.",
        .state = { .type = KB_STATE, .seq = 255, .ack = 255, .intent = KB_INTENT_WITHIN_OPEN, .intent_arg = 255, .nonce = 255,
                   .mode = KB_MODE_WITHIN, .mod = 6, .os = KB_OS_MAC, .os_override = true, .host_alive = true, .event = KB_EVENT_TMUX_KEY },
        .key   = { 12, true, true, true },
    },
    {
        .file  = "minimum",
        .name  = "every field at its minimum",
        .why   = "All zero apart from the type, so a field read from the wrong byte shows up as a stray value.",
        .state = { .type = KB_STATE },
    },
    {
        .file  = "os-override",
        .name  = "the base layer chosen on the keyboard",
        .why   = "Linux picked by hand while the host may say otherwise: the override bit shares a byte with the layer.",
        .state = { .type = KB_STATE, .seq = 8, .ack = 43, .os = KB_OS_LINUX, .os_override = true, .host_alive = true },
    },
    {
        .file  = "within-deep",
        .name  = "a WITHIN_DEEP request",
        .why   = "Asks the host to scroll into Claude's transcript; the nonce is what the host echoes back.",
        .state = { .type = KB_STATE, .seq = 9, .ack = 44, .intent = KB_INTENT_WITHIN_DEEP, .nonce = 4, .mode = KB_MODE_WITHIN, .os = KB_OS_MAC,
                   .host_alive = true },
    },
    {
        .file  = "within-open",
        .name  = "a WITHIN_OPEN request",
        .why   = "Asks the host to open Claude's transcript viewer.",
        .state = { .type = KB_STATE, .seq = 10, .ack = 45, .intent = KB_INTENT_WITHIN_OPEN, .nonce = 5, .mode = KB_MODE_WITHIN, .os = KB_OS_MAC,
                   .host_alive = true },
    },
    {
        .file  = "key-plain",
        .name  = "a tmux key with no modifier",
        .why   = "F13, select-pane -L: the lowest row of the key table.",
        .state = { .type = KB_STATE, .seq = 11, .ack = 46, .mode = KB_MODE_PANE, .os = KB_OS_MAC, .host_alive = true, .event = KB_EVENT_TMUX_KEY },
        .key   = { 1, false, false, false },
    },
    {
        .file  = "key-shift",
        .name  = "a tmux key with shift",
        .why   = "S-F18, next-layout.",
        .state = { .type = KB_STATE, .seq = 12, .ack = 47, .mode = KB_MODE_PANE, .os = KB_OS_MAC, .host_alive = true, .event = KB_EVENT_TMUX_KEY },
        .key   = { 6, true, false, false },
    },
    {
        .file  = "key-ctrl",
        .name  = "a tmux key with ctrl",
        .why   = "C-F13, split-window with SPLIT held.",
        .state = { .type = KB_STATE, .seq = 13, .ack = 48, .mode = KB_MODE_PANE, .mod = 2, .os = KB_OS_MAC, .host_alive = true,
                   .event = KB_EVENT_TMUX_KEY },
        .key   = { 1, false, true, false },
    },
    {
        .file  = "key-alt",
        .name  = "a tmux key with alt",
        .why   = "M-F16, swap-pane right with MOVE held.",
        .state = { .type = KB_STATE, .seq = 14, .ack = 49, .mode = KB_MODE_PANE, .mod = 3, .os = KB_OS_MAC, .host_alive = true,
                   .event = KB_EVENT_TMUX_KEY },
        .key   = { 4, false, false, true },
    },
    {
        .file  = "host-lost",
        .name  = "the host given up on",
        .why   = "Three reports missed: the keyboard says so, and has dropped back out of tmux mode.",
        .state = { .type = KB_STATE, .seq = 15, .ack = 50, .os = KB_OS_MAC },
    },
    {
        .file  = "tree",
        .name  = "TREE mode",
        .why   = "F21 opened choose-tree.",
        .state = { .type = KB_STATE, .seq = 16, .ack = 51, .mode = KB_MODE_TREE, .os = KB_OS_MAC, .host_alive = true,
                   .event = KB_EVENT_TMUX_KEY },
        .key   = { 9, false, false, false },
    },
    {
        .file  = "window",
        .name  = "WINDOW mode with MOVE held",
        .why   = "S-F22, swap-window left.",
        .state = { .type = KB_STATE, .seq = 17, .ack = 52, .mode = KB_MODE_WINDOW, .mod = 3, .os = KB_OS_MAC, .host_alive = true,
                   .event = KB_EVENT_TMUX_KEY },
        .key   = { 10, true, false, false },
    },
    {
        .file  = "within-word",
        .name  = "WITHIN mode with WORD held",
        .why   = "The one held modifier no other case uses.",
        .state = { .type = KB_STATE, .seq = 18, .ack = 53, .mode = KB_MODE_WITHIN, .mod = 5, .os = KB_OS_MAC, .host_alive = true },
    },
    {
        .file  = "reserved",
        .name  = "reserved mode, modifier and request id",
        .why   = "Mode 1, held modifier 4 and request 0x10, the start of the lower reserved range. Nothing sends them; the daemon keeps "
                 "them rather than rejecting the report.",
        .state = { .type = KB_STATE, .seq = 19, .ack = 54, .intent = 0x10, .nonce = 6, .mode = KB_MODE_TMUX, .mod = 4, .os = KB_OS_MAC,
                   .host_alive = true },
    },
    {
        .file  = "reserved-high",
        .name  = "a request id at the top of the reserved range",
        .why   = "0x51, the end of the upper reserved range, kept like any other.",
        .state = { .type = KB_STATE, .seq = 20, .ack = 55, .intent = 0x51, .nonce = 7, .mode = KB_MODE_WITHIN, .os = KB_OS_MAC,
                   .host_alive = true },
    },
};

#define N_CASES (sizeof cases / sizeof cases[0])

static const char *json_bool(bool b) {
    return b ? "true" : "false";
}

// ------------------------------------------------------------ interactions
//
// Conversations rather than single messages: each one replays its steps into
// kb_machine.c and records everything that reached the host, in order. That is
// every STATE, which goes in the .bin, and every row of the tmux key table,
// which tmux hears as a keystroke. Keystrokes for the program in the pane are
// not the host's business and are left out, and so are the layers.

typedef enum { DO_END, DO_CONNECT, DO_CONTEXT, DO_PRESS, DO_HOLD, DO_RELEASE, DO_WAIT } step_kind_t;

typedef struct {
    step_kind_t kind;
    kb_key_t    key;
    uint32_t    ms;
    // The recorder numbers the host's seq itself, and acks the keyboard's latest
    // STATE unless stale_ack says the host has not seen one yet.
    kb_context_t context;
    bool         stale_ack;
} step_t;

#define CONNECT {.kind = DO_CONNECT}
#define CONTEXT(...) {.kind = DO_CONTEXT, .context = {__VA_ARGS__}}
#define STALE_CONTEXT(...) {.kind = DO_CONTEXT, .context = {__VA_ARGS__}, .stale_ack = true}
#define PRESS(k) {.kind = DO_PRESS, .key = KBK_##k}
#define HOLD(k) {.kind = DO_HOLD, .key = KBK_##k}
#define RELEASE(k) {.kind = DO_RELEASE, .key = KBK_##k}
#define WAIT(t) {.kind = DO_WAIT, .ms = (t)}

// A shell pane, tmux present and focused, the host on mac.
#define SHELL .os = KB_OS_MAC, .program = KB_PROGRAM_SHELL, .tmux_bits = KB_TMUX_PRESENT | KB_TMUX_FOCUSED

typedef struct {
    const char *file;
    const char *name;
    const char *why;
    step_t      steps[16];
} interaction_t;

static const interaction_t interactions[] = {
    {
        .file  = "connect",
        .name  = "connecting",
        .why   = "HELLO, then a CONTEXT sent before the host saw it, which the keyboard answers with a STATE, then one that acks it.",
        .steps = {CONNECT, STALE_CONTEXT(SHELL), CONTEXT(SHELL)},
    },
    {
        .file  = "pane-thumbs",
        .name  = "resize, split and swap a pane",
        .why   = "Each PANE thumb held over an arrow: the held modifier is reported on press and release, and rides on the row.",
        .steps = {CONNECT, CONTEXT(SHELL), PRESS(TMUX_ON), HOLD(TP_RSZE), PRESS(TP_UP), RELEASE(TP_RSZE), HOLD(TP_SPLT), PRESS(TP_RGHT),
                  RELEASE(TP_SPLT), HOLD(TP_MOVE), PRESS(TP_LEFT), RELEASE(TP_MOVE)},
    },
    {
        .file  = "tree",
        .name  = "choose from the tree",
        .why   = "TREE opens choose-tree with F21; choosing closes it, which the keyboard reports as PANE.",
        .steps = {CONNECT, CONTEXT(SHELL), PRESS(TMUX_ON), PRESS(TM_TREE), PRESS(TT_SEL)},
    },
    {
        .file  = "window",
        .name  = "move a window, then go to the last one",
        .why   = "WINDOW with MOVE held swaps rather than selects; up is dead while MOVE is held, so it sends nothing.",
        .steps = {CONNECT, CONTEXT(SHELL), PRESS(TMUX_ON), PRESS(TM_WIN), HOLD(TW_MOVE), PRESS(TW_LEFT), PRESS(TW_UP), RELEASE(TW_MOVE),
                  PRESS(TW_LAST)},
    },
    {
        .file  = "claude-viewer",
        .name  = "open Claude's viewer, then ask for the whole conversation",
        .why   = "Two intents, each with its own nonce, answered done and then failed by the host.",
        .steps = {CONNECT,
                  CONTEXT(.os = KB_OS_MAC, .program = KB_PROGRAM_CLAUDE, .tmux_bits = KB_TMUX_PRESENT | KB_TMUX_FOCUSED,
                          .transcript = KB_TRANSCRIPT_CLOSED),
                  PRESS(TMUX_ON), PRESS(TM_WITHIN),
                  CONTEXT(.os = KB_OS_MAC, .program = KB_PROGRAM_CLAUDE, .tmux_bits = KB_TMUX_PRESENT | KB_TMUX_FOCUSED,
                          .transcript = KB_TRANSCRIPT_CLOSED, .intent_nonce = 1, .intent_status = KB_INTENT_PENDING),
                  CONTEXT(.os = KB_OS_MAC, .program = KB_PROGRAM_CLAUDE, .tmux_bits = KB_TMUX_PRESENT | KB_TMUX_FOCUSED,
                          .transcript = KB_TRANSCRIPT_OPEN, .intent_nonce = 1, .intent_status = KB_INTENT_DONE),
                  PRESS(TM_WITHIN),
                  CONTEXT(.os = KB_OS_MAC, .program = KB_PROGRAM_CLAUDE, .tmux_bits = KB_TMUX_PRESENT | KB_TMUX_FOCUSED,
                          .transcript = KB_TRANSCRIPT_OPEN, .intent_nonce = 2, .intent_status = KB_INTENT_FAILED)},
    },
    {
        .file  = "copy-paste",
        .name  = "select a word in copy mode and paste it",
        .why   = "WITHIN on a shell opens copy mode with F19; WORD is reported while held; paste leaves copy mode and tmux mode.",
        .steps = {CONNECT, CONTEXT(SHELL), PRESS(TMUX_ON), PRESS(TM_WITHIN),
                  CONTEXT(.os = KB_OS_MAC, .program = KB_PROGRAM_SHELL, .tmux_bits = KB_TMUX_PRESENT | KB_TMUX_FOCUSED | KB_TMUX_COPY_MODE),
                  HOLD(TC_WORD), PRESS(TC_LEFT), RELEASE(TC_WORD), PRESS(TC_PSTE)},
    },
    {
        .file  = "pane-closed",
        .name  = "the tree closed behind the keyboard",
        .why   = "The host reports no mode open while the keyboard is in TREE with nothing in flight, so the keyboard adopts PANE.",
        .steps = {CONNECT, CONTEXT(SHELL), PRESS(TMUX_ON), PRESS(TM_TREE), CONTEXT(SHELL)},
    },
    {
        .file  = "host-lost",
        .name  = "the host goes quiet",
        .why   = "No CONTEXT for 1.5 s: the next STATE says the host is not alive.",
        .steps = {CONNECT, CONTEXT(SHELL), PRESS(TMUX_ON), WAIT(KB_HOST_TIMEOUT_MS), PRESS(TP_LAST)},
    },
    {
        .file  = "os-key",
        .name  = "follow the host, then force each OS, then follow again",
        .why   = "The host says linux on a keyboard that came up on mac. Following it moves the layer but sends nothing until the next "
                 "STATE: see bugs.md.",
        .steps = {CONNECT, CONTEXT(.os = KB_OS_LINUX, .program = KB_PROGRAM_SHELL, .tmux_bits = KB_TMUX_PRESENT | KB_TMUX_FOCUSED),
                  PRESS(OS_SWAP), PRESS(OS_SWAP), PRESS(OS_SWAP)},
    },
    {
        .file  = "detach",
        .name  = "detach",
        .why   = "C-F17 detaches the client, and the keyboard leaves tmux mode.",
        .steps = {CONNECT, CONTEXT(SHELL), PRESS(TMUX_ON), PRESS(TM_DTCH)},
    },
};

#define N_INTERACTIONS (sizeof interactions / sizeof interactions[0])

static const char *key_names[] = {
    "OTHER",
#define KB_KEY_NAME(name) #name,
    KB_KEYS(KB_KEY_NAME)
#undef KB_KEY_NAME
};

static const char *step_names[] = {"end", "connect", "context", "press", "hold", "release", "wait"};

// What the machine sent during the interaction being recorded.
typedef struct {
    int        step;
    bool       is_row;
    uint8_t    fkey, mods;
    kb_state_t state;
} sent_t;

static sent_t   sent[64];
static int      n_sent;
static uint8_t  sent_bytes[64][KB_REPORT_SIZE];
static int      n_reports;
static int      current_step;
static uint32_t clock_ms;
static bool     overflow;

uint32_t kb_io_now_ms(void) {
    return clock_ms;
}

void kb_io_send(const uint8_t report[KB_REPORT_SIZE], const kb_state_t *state) {
    if (n_sent == 64 || n_reports == 64) {
        overflow = true;
        return;
    }
    memcpy(sent_bytes[n_reports++], report, KB_REPORT_SIZE);
    sent[n_sent++] = (sent_t){.step = current_step, .state = *state};
}

void kb_io_tap_row(uint8_t fkey, uint8_t mods) {
    if (n_sent == 64) {
        overflow = true;
        return;
    }
    sent[n_sent++] = (sent_t){.step = current_step, .is_row = true, .fkey = fkey, .mods = mods};
}

void kb_io_tap_code(uint8_t keycode) {
    (void)keycode;
}

void kb_io_tap_code16(uint16_t keycode) {
    (void)keycode;
}

void kb_io_set_mode_layers(uint8_t mode) {
    (void)mode;
}

void kb_io_set_base(uint8_t os) {
    (void)os;
}

static void receive(kb_context_t c) {
    uint8_t b[KB_REPORT_SIZE] = {KB_MAGIC, KB_VERSION, c.seq, c.ack, KB_CONTEXT, c.os, c.program, c.tmux_bits, c.transcript,
                                 c.phase, c.perm, c.intent_nonce, c.intent_status, c.window, c.pane};
    memcpy(&b[15], c.label, strlen(c.label));
    kb_receive(b, sizeof b);
}

static void print_row(FILE *json, uint8_t fkey, uint8_t mods) {
    fprintf(json, "%s%s%sF%u", mods & MOD_LSFT ? "S-" : "", mods & MOD_LCTL ? "C-" : "", mods & MOD_LALT ? "M-" : "", fkey - KC_F13 + 13);
}

static int record_interaction(FILE *json, const char *dir, const interaction_t *in) {
    kb_machine_init(KB_OS_MAC);
    n_sent = n_reports = 0;
    clock_ms           = 1000;
    uint8_t host_seq   = 0;

    fprintf(json, "    {\n      \"file\": \"interactions/%s.bin\",\n      \"name\": \"%s\",\n      \"why\": \"%s\",\n      \"steps\": [\n", in->file,
            in->name, in->why);
    int n_steps = 0;
    while (n_steps < 16 && in->steps[n_steps].kind != DO_END) n_steps++;

    // The last row tapped, which is the key a TMUX_KEY event describes.
    uint8_t last_fkey = 0, last_mods = 0;
    for (current_step = 0; current_step < n_steps; current_step++) {
        const step_t *s = &in->steps[current_step];
        fprintf(json, "        { \"do\": \"%s\"", step_names[s->kind]);
        switch (s->kind) {
            case DO_CONNECT:
                kb_hello();
                break;
            case DO_CONTEXT: {
                kb_context_t c = s->context;
                c.seq          = ++host_seq;
                c.ack          = s->stale_ack ? KB_SEQ_NONE : kb.my_seq;
                fprintf(json,
                        ", \"fields\": { \"seq\": %u, \"ack\": %u, \"os\": %u, \"program\": %u, \"tmuxBits\": %u, \"transcript\": %u, "
                        "\"phase\": %u, \"perm\": %u, \"intentNonce\": %u, \"intentStatus\": %u, \"window\": %u, \"pane\": %u, \"label\": \"%s\" }",
                        c.seq, c.ack, c.os, c.program, c.tmux_bits, c.transcript, c.phase, c.perm, c.intent_nonce, c.intent_status, c.window,
                        c.pane, c.label);
                receive(c);
                break;
            }
            case DO_PRESS:
                fprintf(json, ", \"key\": \"%s\"", key_names[s->key]);
                kb_key(s->key, true);
                kb_key(s->key, false);
                break;
            case DO_HOLD:
                fprintf(json, ", \"key\": \"%s\"", key_names[s->key]);
                kb_key(s->key, true);
                break;
            case DO_RELEASE:
                fprintf(json, ", \"key\": \"%s\"", key_names[s->key]);
                kb_key(s->key, false);
                break;
            case DO_WAIT:
                fprintf(json, ", \"ms\": %u", (unsigned)s->ms);
                clock_ms += s->ms;
                break;
            case DO_END:
                break;
        }
        fprintf(json, " }%s\n", current_step + 1 < n_steps ? "," : "");
    }
    if (overflow) {
        fprintf(stderr, "%s: more sent than the recorder holds\n", in->file);
        return 1;
    }

    fprintf(json, "      ],\n      \"sent\": [\n");
    int report = 0;
    for (int i = 0; i < n_sent; i++) {
        const sent_t *e = &sent[i];
        fprintf(json, "        { \"step\": %d, ", e->step);
        if (e->is_row) {
            fprintf(json, "\"tmuxKey\": \"");
            print_row(json, e->fkey, e->mods);
            fprintf(json, "\"");
            last_fkey = e->fkey;
            last_mods = e->mods;
        } else {
            const kb_state_t *st = &e->state;
            fprintf(json,
                    "\"report\": %d, \"fields\": { \"type\": %u, \"seq\": %u, \"ack\": %u, \"intent\": %u, \"intentArg\": %u, \"nonce\": %u, "
                    "\"mode\": %u, \"heldModifier\": %u, \"baseLayer\": %u, \"osOverride\": %s, \"hostAlive\": %s, \"event\": %u",
                    report++, st->type, st->seq, st->ack, st->intent, st->intent_arg, st->nonce, st->mode, st->mod, st->os,
                    json_bool(st->os_override), json_bool(st->host_alive), st->event);
            // As in the single messages: a key event's arg is given as the key
            // that went out, here the row tapped just before.
            if (st->event == KB_EVENT_TMUX_KEY) {
                fprintf(json, " }, \"key\": { \"row\": %u, \"shift\": %s, \"ctrl\": %s, \"alt\": %s }", last_fkey - KC_F13 + 1,
                        json_bool(last_mods & MOD_LSFT), json_bool(last_mods & MOD_LCTL), json_bool(last_mods & MOD_LALT));
            } else {
                fprintf(json, ", \"eventArg\": %u }", st->event_arg);
            }
        }
        fprintf(json, " }%s\n", i + 1 < n_sent ? "," : "");
    }
    fprintf(json, "      ]\n    }");

    char path[256];
    snprintf(path, sizeof path, "%s/interactions/%s.bin", dir, in->file);
    FILE *bin = fopen(path, "wb");
    if (!bin || fwrite(sent_bytes, KB_REPORT_SIZE, n_reports, bin) != (size_t)n_reports || fclose(bin) != 0) {
        perror(path);
        return 1;
    }
    return 0;
}

static int record_interactions(const char *dir) {
    char path[256];
    snprintf(path, sizeof path, "%s/interactions", dir);
    if (mkdir(path, 0777) != 0) {
        perror(path);
        return 1;
    }
    snprintf(path, sizeof path, "%s/interactions.json", dir);
    FILE *json = fopen(path, "w");
    if (!json) {
        perror(path);
        return 1;
    }
    fprintf(json,
            "{\n"
            "  \"version\": %d,\n"
            "  \"reportSize\": %d,\n"
            "  \"note\": \"Conversations, each replayed through the keyboard's own state machine, kb_machine.c. steps is what happened: "
            "connect, a CONTEXT from the host (its fields as the daemon names them), a key pressed, held or released, or time passing. "
            "sent is everything that reached the host, in order: each STATE, at index report in the interaction's .bin, and each row of the "
            "tmux key table. Keystrokes for the program in the pane are not included. The keyboard starts on mac each time. Recorded with "
            "test/run.sh record.\",\n"
            "  \"interactions\": [\n",
            KB_VERSION, KB_REPORT_SIZE);
    for (size_t i = 0; i < N_INTERACTIONS; i++) {
        if (record_interaction(json, dir, &interactions[i]) != 0) {
            return 1;
        }
        fprintf(json, "%s\n", i + 1 < N_INTERACTIONS ? "," : "");
    }
    fprintf(json, "  ]\n}\n");
    return fclose(json) == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: record_contract <dir>\n");
        return 2;
    }
    const char *dir = argv[1];
    char        path[256];

    snprintf(path, sizeof path, "%s/expected.json", dir);
    FILE *json = fopen(path, "w");
    if (!json) {
        perror(path);
        return 1;
    }
    fprintf(json,
            "{\n"
            "  \"version\": %d,\n"
            "  \"reportSize\": %d,\n"
            "  \"note\": \"Messages the keyboard sends to the daemon. Each file is one report as it goes over the wire, written by "
            "kb_state_report; the daemon asserts its decoder reads these fields back out of it. Recorded with test/run.sh record.\",\n"
            "  \"messages\": [\n",
            KB_VERSION, KB_REPORT_SIZE);

    for (size_t i = 0; i < N_CASES; i++) {
        const contract_case_t *c = &cases[i];
        kb_state_t             s = c->state;
        if (s.event == KB_EVENT_TMUX_KEY) {
            s.event_arg = kb_tmux_event_arg(c->key.row, c->key.shift, c->key.ctrl, c->key.alt);
        }
        uint8_t buf[KB_REPORT_SIZE];
        kb_state_report(buf, &s);

        snprintf(path, sizeof path, "%s/%s.bin", dir, c->file);
        FILE *bin = fopen(path, "wb");
        if (!bin || fwrite(buf, 1, sizeof buf, bin) != sizeof buf || fclose(bin) != 0) {
            perror(path);
            return 1;
        }

        fprintf(json,
                "    {\n"
                "      \"file\": \"%s.bin\",\n"
                "      \"name\": \"%s\",\n"
                "      \"why\": \"%s\",\n"
                "      \"fields\": {\n"
                "        \"type\": %u,\n"
                "        \"seq\": %u,\n"
                "        \"ack\": %u,\n"
                "        \"intent\": %u,\n"
                "        \"intentArg\": %u,\n"
                "        \"nonce\": %u,\n"
                "        \"mode\": %u,\n"
                "        \"heldModifier\": %u,\n"
                "        \"baseLayer\": %u,\n"
                "        \"osOverride\": %s,\n"
                "        \"hostAlive\": %s,\n"
                "        \"event\": %u",
                c->file, c->name, c->why, s.type, s.seq, s.ack, s.intent, s.intent_arg, s.nonce, s.mode, s.mod, s.os, json_bool(s.os_override),
                json_bool(s.host_alive), s.event);
        // A key event's arg is the key, given as what went in rather than the
        // byte the encoder made of it.
        if (s.event == KB_EVENT_TMUX_KEY) {
            fprintf(json,
                    "\n      },\n"
                    "      \"key\": { \"row\": %u, \"shift\": %s, \"ctrl\": %s, \"alt\": %s }\n",
                    c->key.row, json_bool(c->key.shift), json_bool(c->key.ctrl), json_bool(c->key.alt));
        } else {
            fprintf(json, ",\n        \"eventArg\": %u\n      }\n", s.event_arg);
        }
        fprintf(json, "    }%s\n", i + 1 < N_CASES ? "," : "");
    }
    fprintf(json, "  ]\n}\n");
    if (fclose(json) != 0) {
        return 1;
    }
    return record_interactions(dir);
}

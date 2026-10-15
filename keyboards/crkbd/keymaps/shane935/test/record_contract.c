// Records what kb_state_report emits, as the contract the daemon verifies its
// decoder against. The cases are chosen by hand; the bytes are whatever the
// encoder produces. Each .bin is one report exactly as it goes over the wire,
// and expected.json holds the values that went in.
//
//   test/run.sh record
//
// Deliberate, never automatic: the committed files are the agreement, so a
// change to them has to appear in a diff and be explained. A plain run checks
// they still match the encoder.

#include "../kb_protocol.h"

#include <stdio.h>
#include <string.h>

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
    return fclose(json) == 0 ? 0 : 1;
}

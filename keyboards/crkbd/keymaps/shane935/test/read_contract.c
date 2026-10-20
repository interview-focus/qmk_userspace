// Reads the daemon's contract/to-keyboard/output.bin with kb_context_parse and
// prints what it got as JSON, one entry per 32-byte report, using the daemon's
// field names. check_contract.mjs compares that with the daemon's input.json.
// A report the parser rejects prints as null.
//
//   read_contract <dir holding output.bin>

#include "../kb_protocol.h"

#include <stdio.h>

static void print_label(const char *s) {
    putchar('"');
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            printf("\\%c", c);
        } else if (c < 0x20 || c >= 0x7f) {
            printf("\\u%04x", c);
        } else {
            putchar(c);
        }
    }
    putchar('"');
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: read_contract <dir>\n");
        return 2;
    }
    char path[256];
    snprintf(path, sizeof path, "%s/output.bin", argv[1]);
    FILE *bin = fopen(path, "rb");
    if (!bin) {
        perror(path);
        return 1;
    }

    printf("{\"version\":%d,\"reportSize\":%d,\"messages\":[", KB_VERSION, KB_REPORT_SIZE);
    uint8_t buf[KB_REPORT_SIZE];
    size_t  n;
    for (int i = 0; (n = fread(buf, 1, sizeof buf, bin)) > 0; i++) {
        if (i > 0) putchar(',');
        kb_context_t c;
        if (n != sizeof buf || !kb_context_parse(buf, (uint8_t)n, &c)) {
            printf("null");
            continue;
        }
        printf("{\"seq\":%u,\"ack\":%u,\"os\":%u,\"program\":%u,\"tmuxBits\":%u,\"transcript\":%u,\"phase\":%u,\"perm\":%u,"
               "\"intentNonce\":%u,\"intentStatus\":%u,\"window\":%u,\"pane\":%u,\"label\":",
               c.seq, c.ack, c.os, c.program, c.tmux_bits, c.transcript, c.phase, c.perm, c.intent_nonce, c.intent_status, c.window,
               c.pane);
        print_label(c.label);
        putchar('}');
    }
    printf("]}\n");
    return fclose(bin) == 0 ? 0 : 1;
}

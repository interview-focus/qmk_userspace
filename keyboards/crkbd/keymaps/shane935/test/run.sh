#!/bin/sh
# Builds and runs the unit tests for the pure parts of this keymap: kb_protocol.c
# (the wire format) and tmux_context.c (what WITHIN mode sends). Neither calls
# into QMK -- tmux_context.c includes QMK's keycode headers, which are enums and
# macros only -- so this needs a C compiler and nothing else: no QMK build, no
# toolchain, no keyboard.
#
# With a native compiler it just builds and runs. Without one, point WASI_SDK at
# an unpacked wasi-sdk and it builds for wasm32-wasip1 and runs under node
# instead, which is how it is tested in the agent sandbox:
#
#   curl -sL -o /tmp/w.tar.gz https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-25/wasi-sdk-25.0-x86_64-linux.tar.gz
#   tar xzf /tmp/w.tar.gz -C /tmp
#   WASI_SDK=/tmp/wasi-sdk-25.0-x86_64-linux test/run.sh
#
# It also checks contract/to-daemon/ still matches what kb_state_report emits.
# `test/run.sh record` rewrites it, which is deliberate: see record_contract.c.
#
# With KEYBOARD_DAEMON pointing at a checkout of keyboard-daemon, it also checks
# kb_context_parse reads the daemon's contract/to-keyboard/ back as the daemon
# says it should. That needs node, wasm or not. contract/to-keyboard.verified
# notes which daemon contract last passed; `record` updates it.
set -e
here=$(dirname "$0")
root="$here/../../../../.."
tmp="${TMPDIR:-/tmp}"
warn="-std=c11 -Wall -Wextra -Werror"
# QMK's keycode headers, for the keycodes tmux_context.c resolves to.
inc="-I$root/quantum -I$root/quantum/keymap_extras -I$root/quantum/sequencer"
srcs="$here/../kb_protocol.c $here/../tmux_context.c $here/../kb_rules.c"

if [ -n "$WASI_SDK" ]; then
    cat > "$tmp/run.mjs" <<'JS'
import { WASI } from 'node:wasi';
import { readFile } from 'node:fs/promises';
// An optional directory is opened as /out, the only path the program can reach.
const [wasm, dir] = process.argv.slice(2);
const wasi = new WASI({
    version: 'preview1',
    args: dir ? ['test', '/out'] : ['test'],
    env: {},
    preopens: dir ? { '/out': dir } : {},
});
const mod = await WebAssembly.compile(await readFile(wasm));
process.exitCode = wasi.start(await WebAssembly.instantiate(mod, wasi.getImportObject()));
JS
fi

status=0

# keymap.c cannot be compiled without QMK, but its preprocessor directives can
# be checked without it, and an #ifdef that loses its #else is exactly the kind
# of break that survives a careful read. All four combinations, because the
# RAW_ENABLE-off branch defines send_state as a macro and is never built here.
cc_pp="${CC:-cc}"
pp_target=""
if [ -n "$WASI_SDK" ]; then
    cc_pp="$WASI_SDK/bin/clang"
    pp_target="--target=wasm32-wasip1"
fi
mkdir -p "$tmp/ppstub"
: > "$tmp/ppstub/qmk_stub.h"
for raw in -DRAW_ENABLE -URAW_ENABLE; do
    for oled in -DOLED_ENABLE -UOLED_ENABLE; do
        # shellcheck disable=SC2086
        if ! "$cc_pp" -E -P $pp_target -DQMK_KEYBOARD_H='"qmk_stub.h"' "$raw" "$oled" -I"$tmp/ppstub" -I"$here/.." $inc "$here/../keymap.c" -o /dev/null; then
            echo "keymap.c: preprocessor failed with $raw $oled"
            status=1
        fi
    done
done
[ "$status" = 0 ] && echo "keymap.c: directives balanced in all four builds"

for t in "$here"/*_test.c; do
    name=$(basename "$t" .c)
    if [ -n "$WASI_SDK" ]; then
        # shellcheck disable=SC2086
        "$WASI_SDK/bin/clang" --target=wasm32-wasip1 $warn $inc -o "$tmp/$name.wasm" "$t" $srcs
        node --no-warnings "$tmp/run.mjs" "$tmp/$name.wasm" || status=1
    else
        # shellcheck disable=SC2086
        "${CC:-cc}" $warn $inc -o "$tmp/$name" "$t" $srcs
        "$tmp/$name" || status=1
    fi
done
contract="$here/../contract/to-daemon"
out="$tmp/to-daemon"
[ "$1" = record ] && out="$contract"
rm -rf "$out"
mkdir -p "$out"
if [ -n "$WASI_SDK" ]; then
    # shellcheck disable=SC2086
    "$WASI_SDK/bin/clang" --target=wasm32-wasip1 $warn -o "$tmp/record_contract.wasm" "$here/record_contract.c" "$here/../kb_protocol.c"
    node --no-warnings "$tmp/run.mjs" "$tmp/record_contract.wasm" "$out" || status=1
else
    # shellcheck disable=SC2086
    "${CC:-cc}" $warn -o "$tmp/record_contract" "$here/record_contract.c" "$here/../kb_protocol.c"
    "$tmp/record_contract" "$out" || status=1
fi
if [ "$1" = record ]; then
    echo "contract: recorded to-daemon"
elif diff -r "$contract" "$out"; then
    echo "contract: to-daemon matches the encoder"
else
    echo "contract: to-daemon is stale; run test/run.sh record and explain the diff"
    status=1
fi

daemon_contract="${KEYBOARD_DAEMON:+$KEYBOARD_DAEMON/contract/to-keyboard}"
verified="$here/../contract/to-keyboard.verified"
if [ -z "$daemon_contract" ]; then
    last=$(sed -n 's/.*"daemonCommit": "\(.*\)".*/\1/p' "$verified" 2>/dev/null || true)
    echo "contract: to-keyboard skipped, last passed against daemon ${last:-nothing}; set KEYBOARD_DAEMON to a keyboard-daemon checkout"
else
    read_out="$tmp/to-keyboard.json"
    if [ -n "$WASI_SDK" ]; then
        # shellcheck disable=SC2086
        "$WASI_SDK/bin/clang" --target=wasm32-wasip1 $warn -o "$tmp/read_contract.wasm" "$here/read_contract.c" "$here/../kb_protocol.c"
        node --no-warnings "$tmp/run.mjs" "$tmp/read_contract.wasm" "$daemon_contract" > "$read_out" || status=1
    else
        # shellcheck disable=SC2086
        "${CC:-cc}" $warn -o "$tmp/read_contract" "$here/read_contract.c" "$here/../kb_protocol.c"
        "$tmp/read_contract" "$daemon_contract" > "$read_out" || status=1
    fi
    if node "$here/check_contract.mjs" "$daemon_contract" "$read_out" "$verified" "$1"; then
        echo "contract: to-keyboard reads back as the daemon recorded it"
    else
        status=1
    fi
fi

exit $status

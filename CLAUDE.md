This is the QMK repo for flashing my keyboard it is a clone of the main QMK repo.

The only code that has any relvence to us is the keyboard setup at: keyboards/crkbd/keymaps/shane935

## Talking about key positions

Use `L(col,row)` / `R(col,row)` for the left and right halves, numbered exactly
as the keys are printed in keymap.c: col 1-5 left to right, row 1-3 top to
bottom. Thumbs are `LT1-3` / `RT1-3`, also in source order.

Right hand columns map to fingers like this (base-layer keys in brackets):

| col | finger              | keys      |
|-----|---------------------|-----------|
| R1  | index, inner stretch| Y / H / N |
| R2  | index               | U / J / M |
| R3  | middle              | I / K / , |
| R4  | ring                | O / L / . |
| R5  | pinky               | P / ; / ' |

So "PgUp on R(5,1)" means the right pinky, top row.

## Layers

`enum layers` is 0-indexed: _MAC(0), _LINUX(1), _NUM_MAC(2), _NUM_LINUX(3),
_NAV_MAC(4), _NAV_LINUX(5), _TMUX(6), _TMUX_TREE(7), _TMUX_WINDOW(8),
_TMUX_PANE(9), _TMUX_WITHIN(10). Prefer the names over the numbers.

The six OS layers are duplicated in pairs. A change to one half of a pair almost
always needs the same change to the other, or the two OS modes drift apart. Ask
before assuming a change is meant for only one.

None of the five tmux layers is duplicated: a function key means the same thing
to tmux whichever OS it is. _TMUX carries the mode row and EXIT and is on
whenever tmux mode is on, with exactly one of TREE, WINDOW, PANE or WITHIN above
it — so _TMUX is never on alone, and the mode row shows through as transparent
on all four.

## Reference site

`keyboards/crkbd/keymaps/shane935/site/` generates a reference page from
keymap.c. It is not committed: build it with `node site/build.mjs`, and rebuild
it after changing the keymap. `node site/build.mjs --check` fails if it is
stale.

Behaviour that is not in the `keymaps[]` array — OS_SWAP, the both-shifts caps
word chord, the tapping term — is hand-written in `site/annotations.mjs` and
has to be updated by hand when that behaviour changes.

## Tests

`kb_protocol.c` (the wire format), `kb_rules.c` (how reports change desired
state) and `tmux_context.c` (what WITHIN mode sends) are the parts that need no
QMK, and `test/run.sh` builds and runs them. It also preprocesses `keymap.c` in
all four RAW_ENABLE/OLED_ENABLE combinations, which catches an unbalanced
`#ifdef` without needing a QMK build — counting directives by eye does not,
because it misses `#else`.

There is no C compiler in the agent sandbox. Point `WASI_SDK` at an unpacked
wasi-sdk and `run.sh` builds for wasm32-wasip1 and runs under node instead; the
fetch command is in the script's header.

`contract/` is the agreement with keyboard-daemon. `to-daemon/` is what this
keyboard sends, and `run.sh` fails if it no longer matches the encoder;
`run.sh record` rewrites it. Set `KEYBOARD_DAEMON` to a keyboard-daemon
checkout and `run.sh` also parses that repo's `contract/to-keyboard/`;
`to-keyboard.verified` notes which daemon contract last passed, and `record`
updates it. QMK's root `.gitignore` drops `*.bin`, and `contract/.gitignore`
is what keeps the reports committed.

`keymap.c` itself only compiles under QMK, because `quantum/keymap_introspection.c`
is the translation unit that includes it.

## The protocol

`docs/` holds all three specs and is where all three are edited: `00-protocol.md`
is the wire contract, `20-daemon.md` the host daemon, `30-dotfiles.md` the tmux
and Claude Code config. The daemon and dotfiles repos take copies from here, so a
change goes in here first and is copied out. `VERSION` in `00-protocol.md` tracks
the wire format and the key table, not the prose.

There is no separate firmware spec: the code is it. `tmux_context.c` holds what
WITHIN sends, `kb_rules.c` how reports change desired state, `kb_protocol.c` the
wire format, and each has a test beside it. What is left in `keymap.c` is wiring,
and only a flashed board can check that.

`bugs.md` is what is known to be wrong and deliberately not fixed, with what
each fix would take.

The keyboard owns desired state — which tmux mode, which modifier, which base
layer — and the host owns observable context: the program in the focused pane,
whether it is in a tmux mode, and the OS, which is the one thing the host is
authoritative about. Reports are inputs to the keyboard's own rules, never
commands.

Every tmux *action* — anything that changes what tmux is showing — is a single
root-table key from the table in that file, so the commands and all their flags
live in the daemon's generated tmux.conf and not in `keymap.c`. Keys sent to a
tmux mode that is already open are the exception and go straight through:
choose-tree's own x, y, Enter and arrows, and copy-mode-vi's motions. The
generated conf binds the copy-mode-vi keys WITHIN relies on explicitly so they
do not depend on `mode-keys`.

There are two intents, WITHIN_DEEP and WITHIN_OPEN, and both are about Claude's
transcript viewer, which the keyboard has no way to observe. The host is asked
only where it can do something the keyboard cannot: wait for the conversation to
stop being written, or retry a Ctrl-O a busy pane swallowed. Everything else the
keyboard decides itself from the last report.

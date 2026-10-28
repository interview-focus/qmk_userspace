# Known bugs

Things that are wrong and not worth fixing yet. Each one says what it takes to
fix, so the decision does not have to be made again from scratch.

## A host that dies mid-WITHIN sends copy-mode keys to the program

Entering WITHIN offline is fine: the target resolves to `copy` and `F19` opens
copy mode, so the keys land in copy mode where they belong. The problem is the
host dying while WITHIN is *already* open.

After 1500 ms the target flips to `copy`, but nothing re-runs the entry, so copy
mode is not open — and the copy column's letters go straight to whatever is in
the pane. `b`, `w`, `0`, `$`, `n`, `N`, `V` and `Enter`, to a program that was
being driven by the transcript or hunk column a moment ago.

The sharpest case is a Claude pane. `claude-safe` exists so that nothing can put
a letter into a live prompt, and that guarantee holds only while the host is
alive: pull the cable and WORD+← types a `b` into the message box. `q` is still
unreachable from any column, so hunk cannot be quit.

**Fix:** notice the alive→dead edge and re-run `within_send(WK_ENTER)` while
`kb.mode == KB_MODE_WITHIN`. That opens copy mode for real and makes the offline
column honest. Needs a flag for the previous liveness, since `kb_alive()` is
computed rather than stored. Liveness goes stale on a timer, not on a report,
so `kb_receive` never sees the edge: it needs a new `kb_tick` in
`kb_machine.c`, called from `housekeeping_task_user`. `kb_machine_test.c` can
drive it by moving its clock.

## A base layer the host moved is not reported until the next keypress

STATE is sent "on every change to an owned field", and byte 10, the base layer,
is one the keyboard owns. `kb_apply_os` runs on every CONTEXT and can move the
layer, but nothing after it sends STATE: the only STATE leaving
`kb_receive` is the stale-ack resend, and a steady-state CONTEXT acks the
current seq, so that does not fire.

It shows whenever the host's reported `os` changes while the source is
`KB_OS_FOLLOW` — a KVM switch, or the daemon coming up against a different
machine. The keyboard moves the layer and the host's `@kb_os` keeps the old value
until the next key that sends STATE. First connect escapes it by accident: the
first CONTEXT usually has not seen HELLO yet, so its ack is stale and the resend
carries the new layer anyway.

Minor, because the host supplied the `os` that moved the layer and
`kb_os_wanted` is pure, so a host deriving the value rather than reading byte 10
never sees it. Only a host that mirrors the byte — which is what the protocol
tells it to do — lags.

**Fix:** return from `kb_apply_os` whether it moved the layer, and send STATE for
it in `kb_receive`. The caller has to do that rather than `kb_apply_os` itself,
because OS_SWAP already sends unconditionally and has to keep doing so: tapping
off Follow changes byte 10's bit 7 even when the layer stays put. A test in
`kb_machine_test.c` can pin it: `test_follow_os` already sees the layer move.

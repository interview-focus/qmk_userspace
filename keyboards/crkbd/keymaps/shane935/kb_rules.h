// The rules by which the host's reports change the keyboard's desired state.
//
// Pulled out of keymap.c for the same reason as kb_protocol.c: every one of
// these is a decision with an edge case, and none of them needs QMK to make it.
// A wrong term in kb_adopt_pane drops you out of copy mode at random; a wrong
// bit in kb_tmux_row_mods sends the unmodified key and resizes nothing. Neither
// is visible from reading the call site. test/run.sh builds them natively.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "kb_protocol.h"

// Where the base layer comes from. The host is the authority on which OS it is
// running, and the two forces are what outrank it. The OS key cycles all three
// in this order, so the way out of an override is the key that got you into one.
enum kb_os_source { KB_OS_FOLLOW, KB_OS_FORCE_MAC, KB_OS_FORCE_LINUX };

// The source the OS key selects next. Three taps comes back to following.
uint8_t kb_os_next(uint8_t source);

// Which OS the base layer should be on, or KB_OS_UNKNOWN to leave it where it
// is -- which is what following a host that has not reported yet means.
uint8_t kb_os_wanted(uint8_t source, uint8_t reported_os);

// The host counts as alive only while a CONTEXT has landed inside the timeout.
// A last_ms of zero means none ever has, which is why it is not the same as one
// landing at tick zero: without that the first 1.5 s after boot reads as alive.
bool kb_host_alive(uint32_t last_ms, uint32_t now_ms);

// The contradiction rule. True when the host reports no tmux mode open while the
// keyboard believes it is in one and no intent is in flight to explain it, which
// means a pane closed or a program exited behind the keyboard's back.
// claims_mode is the keyboard's side of it: desired is TREE, or WITHIN driving
// copy mode. Requires tmux to be present, so a report from outside tmux -- where
// nothing is open because there is nothing to open -- cannot trigger it.
bool kb_adopt_pane(const kb_context_t *ctx, bool claims_mode);

// True when the host has finished with the intent this nonce belongs to, rather
// than when it is still working or when the status refers to an older one.
bool kb_intent_settled(const kb_context_t *ctx, uint8_t nonce);

// A resolved keycode that names a row of the tmux key table: F13 to F24, alone
// or with any of left ctrl, shift and alt. Those go out through tmux_fkey so the
// host hears the event; anything else is a keystroke for the program.
bool kb_is_tmux_row(uint16_t keycode);

// The function key and the MOD_ bits to send it with. Only meaningful for a
// keycode kb_is_tmux_row accepts.
uint8_t kb_tmux_row_key(uint16_t keycode);
uint8_t kb_tmux_row_mods(uint16_t keycode);

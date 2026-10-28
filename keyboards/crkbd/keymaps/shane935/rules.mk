OLED_ENABLE = yes
CAPS_WORD_ENABLE = yes
RAW_ENABLE = yes

# The atmega32u4 has 28672 bytes and Raw HID wants its share. These two are the
# only savings that cost nothing: LTO is an optimisation rather than a feature,
# and nothing can reach mouse keys from this keymap -- there is no KC_MS_, KC_WH_
# or KC_BTN on any layer, so the pointer code and its HID report are dead weight.
# Everything else the board switches on is left alone, media keys and NKRO and
# the LEDs included, so adding a key for any of them later is a keymap change.
LTO_ENABLE = yes
MOUSEKEY_ENABLE = no

# The wire format, kept out of keymap.c so it can be built and tested without
# QMK. test/run.sh does exactly that.
SRC += kb_protocol.c
SRC += tmux_context.c
SRC += kb_rules.c
SRC += kb_machine.c

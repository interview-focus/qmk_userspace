// The half of this keymap that is not in the keymaps[] array.
//
// Everything here is written by hand, because no parser can read it out of the
// C: OS_SWAP's effect lives in process_record_user, the Caps Word chord lives
// in config.h and has no array entry at all, and the tapping term is a QMK
// default that config.h leaves commented out. Keep it accurate by hand.
//
// The tmux keycodes are the bulk of it. Every one of them is a case in
// process_record_user that sends one row of the key table in keyboard-daemon's
// src/keytable.ts, and the array shows only the name, so customKeys below is
// the only place the page learns what they do.
//
// A module rather than JSON so it can carry comments and multi-line prose.

// tmux mode is one state with four faces, so all four tabs say the same thing
// about it.
const TMUX_BANNER = {
    title: 'tmux mode is a toggle, not a hold.',
    body:
        'It stays on until Exit or Detach, and everything hatched below is dead while it is. ' +
        'The four mode keys and Exit live on the shared _TMUX layer underneath, which is why ' +
        'they show through as transparent on all four modes and sit on the same keys in each. ' +
        'The OLED prints which mode you are in, and which thumb modifier is live.',
};

export default {
    // Which layers pair up, and what to call them. decorate.mjs resolves
    // transparency against `baseGroup`, through `under` first where it is set.
    //
    // _TMUX has no tab of its own: it is never on without exactly one mode
    // layer above it, so a tab for it would be a state you cannot be in. It is
    // claimed by being stacked under all four modes instead.
    baseGroup: 'base',
    groups: [
        { id: 'base', title: 'Base', short: 'BASE', mac: '_MAC', linux: '_LINUX' },
        { id: 'num', title: 'Numbers', short: 'NUM', mac: '_NUM_MAC', linux: '_NUM_LINUX' },
        { id: 'nav', title: 'Nav', short: 'NAV', mac: '_NAV_MAC', linux: '_NAV_LINUX' },
        // Pane first: it is the mode tmux mode starts in.
        { id: 'pane', title: 'Tmux · Pane', short: 'PANE', shared: '_TMUX_PANE', under: ['_TMUX'] },
        { id: 'window', title: 'Tmux · Window', short: 'WINDOW', shared: '_TMUX_WINDOW', under: ['_TMUX'] },
        { id: 'tree', title: 'Tmux · Tree', short: 'TREE', shared: '_TMUX_TREE', under: ['_TMUX'] },
        { id: 'within', title: 'Tmux · Within', short: 'WITHIN', shared: '_TMUX_WITHIN', under: ['_TMUX'] },
    ],

    meta: {
        tappingTerm: 200,
        tappingTermNote:
            'QMK default — config.h leaves TAPPING_TERM commented out. Hold a home-row key ' +
            'past 200 ms and you get the modifier; release sooner and you get the letter.',
        tapFlavour:
            'No PERMISSIVE_HOLD, HOLD_ON_OTHER_KEY_PRESS or QUICK_TAP_TERM are set, so a fast ' +
            'roll across the home row resolves as taps rather than firing a modifier.',
        configFlags: ['SPLIT_LAYER_STATE_ENABLE', 'BOTH_SHIFTS_TURNS_ON_CAPS_WORD'],
        rules: ['OLED_ENABLE', 'CAPS_WORD_ENABLE', 'RAW_ENABLE'],
    },

    groupProse: {
        base: 'QWERTY with the outer pinky columns removed. The home row doubles as modifiers.',
        num: 'Numpad-style digits under the left hand, symbols down the right of it, and the ' +
            'right home row becomes bare modifiers so you can chord them with the digits.',
        nav: 'Arrows under the right index/middle/ring, editing keys below them, and bare ' +
            'modifiers on the left home row to combine with those arrows.',
        pane: 'Where tmux mode starts. The arrows move between panes, and the three left ' +
            'thumbs are held modifiers that turn those same arrows into resize, split and swap.',
        window: 'Left and right move between windows, up and down between sessions. The two ' +
            'left thumbs are held modifiers for making and moving windows.',
        tree: 'choose-tree, driven by its own key table, so most of this layer is plain keys ' +
            'sent straight through.',
        within: 'Inside whatever the focused pane is running, which the host tells the keyboard: ' +
            "tmux's own scrollback, Claude's transcript viewer, a Claude pane whose viewer is shut, " +
            'or hunk. Not one key here has a fixed meaning — the OLED names the target, and that ' +
            'is what every key on the layer is resolved against. The two left thumbs are held ' +
            'modifiers that say how big a step to take.',
    },

    groupBanner: {
        pane: TMUX_BANNER,
        window: TMUX_BANNER,
        tree: TMUX_BANNER,
        within: TMUX_BANNER,
    },

    // Keyed by custom keycode, because the behaviour follows the keycode rather
    // than the position: TM_TREE means the same thing on all four tmux layers.
    // decorate.mjs fails the build on a custom keycode that is missing here, and
    // on an entry here for a keycode keymap.c no longer has.
    //
    // `hold` instead of `label` means the key is held rather than tapped.
    // `target` is the layer the key opens, and is what the page derives "how you
    // get to this layer" from -- so only the keys whose job is to open a layer
    // carry one.
    customKeys: {
        OS_SWAP: {
            label: 'OS',
            desc: 'Cycle: follow the host, force Mac, force Linux',
            note: 'The host reports which OS it is and the keyboard follows, so this key is no ' +
                'longer how you switch day to day — it is how you overrule the report, and how ' +
                'you give it back. It cycles three ways: following the host, forced to Mac, ' +
                'forced to Linux, and round to following again, so three taps always returns ' +
                'you to the host whatever state you were in. While either force is on the OLED ' +
                'says LOCK, which matters because the first tap off following often does not ' +
                'move the layer at all — it is already where the host put it. The layer itself ' +
                'is written to EEPROM as it always was and survives unplugging; the lock is not, ' +
                'so a replug comes back up following the host. Nav layer only.',
        },

        // --- getting in and out of tmux mode -------------------------------
        TMUX_ON: {
            label: 'tmux',
            desc: 'Turn tmux mode on, in Pane mode',
            target: '_TMUX_PANE',
            note: 'Turns tmux mode on and lands you in Pane mode. It is a toggle, not a hold: ' +
                'tmux mode stays on until Exit or Detach, and while it is on almost every ' +
                'other key is dead.',
        },
        TM_TREE: {
            label: 'Tree',
            desc: "Tree mode — tmux's session and window tree",
            target: '_TMUX_TREE',
            note: 'Opens choose-tree -Zw -O activity (zoomed, windows included, most recently ' +
                'used first) and switches the keyboard to Tree mode. Tapping it again while ' +
                'already in Tree mode quits the tree and drops back to Pane.',
        },
        TM_WIN: {
            label: 'Window',
            desc: 'Window mode — move between windows and sessions',
            target: '_TMUX_WINDOW',
            note: 'Switches the keyboard to Window mode. Nothing is opened in tmux itself — ' +
                'Window mode only changes what the keys send. Pressing it when you are ' +
                'already in Window mode does nothing.',
        },
        TM_PANE: {
            label: 'Pane',
            desc: 'Pane mode — move, split and resize panes',
            target: '_TMUX_PANE',
            note: 'Switches the keyboard to Pane mode, the mode tmux mode starts in. Like ' +
                'Window mode it opens nothing; it only changes what the keys send.',
        },
        TM_WITHIN: {
            label: 'Within',
            desc: 'Within mode — navigate inside the pane',
            target: '_TMUX_WITHIN',
            note: 'Switches the keyboard to Within mode and does whatever the target needs to ' +
                'become navigable: copy-mode for a shell, nothing at all for a viewer already ' +
                'open or for hunk, and for a Claude pane whose viewer is shut, an ask — the host ' +
                'sends the Ctrl-O, watches whether the viewer actually opened, and tries again if ' +
                'it did not. A busy pane can swallow that key and the keyboard would never know. ' +
                'Which target it is comes from the host, not from a key: there is no App key any ' +
                'more and nothing to cycle. The OLED shows ? until the viewer is confirmed open. ' +
                'Pressing it again once the viewer is open is a second ask, for the whole ' +
                'conversation: Claude writes it into the terminal scrollback and the host opens ' +
                'copy mode over all of it, so you can search and yank across the lot rather than ' +
                'the one frame Claude is drawing. So this key winds in while there is further in ' +
                'to go and comes out of copy mode when there is not. Over a Claude viewer that ' +
                'lands you back in the viewer, with the next press offering the conversation ' +
                'again — in and back out. Anywhere else there is nothing underneath copy mode, so ' +
                'it drops to Pane rather than leaving Within with nothing open.',
        },
        TM_EXIT: {
            label: 'Exit',
            desc: 'Leave tmux mode',
            note: 'Turns tmux mode off and gives the keyboard back. If a tree or copy mode is ' +
                'open it sends F20 first, so the next thing you type lands in the shell rather ' +
                'than in a mode that was left open. It is the only key that gets you out ' +
                'without touching tmux state, so it is worth memorising its position.',
        },
        TM_DTCH: {
            label: 'Detach',
            desc: 'Detach the client, then leave tmux mode',
            note: 'Sends Ctrl-F17, detach-client, and turns tmux mode off with it — there is no ' +
                'longer a session to send keys to. It sits on the pinky home row, away from ' +
                'the top row it used to share with Zoom and Layout, because detaching by ' +
                'accident costs you the whole session.',
        },

        // --- Pane mode -----------------------------------------------------
        TP_UP: {
            label: '↑',
            desc: 'Focus the pane above',
            note: 'F15, select-pane -U, on its own. The modifiers send the same key with Shift, ' +
                'Ctrl or Alt on it: resize-pane -U 5, split-window -vb, and swap-pane -s ' +
                '{up-of} — which takes the neighbour as -s rather than -t so focus ends up on ' +
                'the pane that moved, which is what lets repeated presses push the same pane along.',
        },
        TP_DOWN: {
            label: '↓',
            desc: 'Focus the pane below',
            note: 'F14, select-pane -D; resize-pane -D 5 with Resize, split-window -v with ' +
                'Split, swap-pane -s {down-of} with Move.',
        },
        TP_LEFT: {
            label: '←',
            desc: 'Focus the pane to the left',
            note: 'F13, select-pane -L; resize-pane -L 5 with Resize, split-window -hb with ' +
                'Split, swap-pane -s {left-of} with Move.',
        },
        TP_RGHT: {
            label: '→',
            desc: 'Focus the pane to the right',
            note: 'F16, select-pane -R; resize-pane -R 5 with Resize, split-window -h with ' +
                'Split, swap-pane -s {right-of} with Move.',
        },
        TP_LAST: {
            label: 'Last',
            desc: 'Back to the pane you were in before',
            note: 'F17 — select-pane -l.',
        },
        TP_ZOOM: {
            label: 'Zoom',
            desc: 'Zoom this pane in or out',
            note: 'F18, resize-pane -Z. Zooming fills the window with this pane; tapping it ' +
                'again puts the others back.',
        },
        TP_LYT: {
            label: 'Layout',
            desc: 'Cycle the window layout',
            note: "Shift-F18, next-layout, which steps through tmux's preset layouts.",
        },
        TP_RSZE: {
            hold: { label: 'Resize', desc: 'Held: the arrows resize the pane' },
            note: 'Held, not tapped. While it is down the four arrows send resize-pane in ' +
                '5-cell steps instead of moving the focus. Releasing it clears the modifier, ' +
                'and so does changing mode.',
        },
        TP_SPLT: {
            hold: { label: 'Split', desc: 'Held: the arrows split the pane' },
            note: 'Held. The arrows become split-window in that direction, each with ' +
                "-c '#{pane_current_path}' so the new pane opens where the current one is. " +
                'Up and left add -b, which puts the new pane before the current one.',
        },
        TP_MOVE: {
            hold: { label: 'Move', desc: 'Held: the arrows swap this pane with its neighbour' },
            note: 'Held. The arrows become swap-pane against the neighbour in that direction. ' +
                'Window mode has a Move on the same thumb and neither can be live while the ' +
                'other is, so the two share one state.',
        },

        // --- Window mode ---------------------------------------------------
        TW_UP: {
            label: '↑',
            desc: 'Previous session',
            note: 'Shift-F19, switch-client -p. Up and down move between sessions here; left ' +
                'and right move between windows. Does nothing while Move is held — Move only ' +
                'applies to windows.',
        },
        TW_DOWN: {
            label: '↓',
            desc: 'Next session',
            note: 'Shift-F20, switch-client -n. Does nothing while Move is held.',
        },
        TW_LEFT: {
            label: '←',
            desc: 'Previous window',
            note: 'F22, select-window -p. With Move held it is Shift-F22, swap-window -d -t -1, ' +
                'where -d is what makes the client follow the window it moved.',
        },
        TW_RGHT: {
            label: '→',
            desc: 'Next window',
            note: 'F23, select-window -n. With Move held it is Shift-F23, swap-window -d -t +1.',
        },
        TW_LAST: {
            label: 'Last',
            desc: 'Back to the window you were in before',
            note: 'Shift-F21 — select-window -l.',
        },
        TW_MOVE: {
            hold: { label: 'Move', desc: 'Held: left and right move this window' },
            note: "Held. Shares its state with Pane mode's Move on the same thumb; only one " +
                'of the two modes can be live at a time.',
        },

        // --- Within mode ---------------------------------------------------
        //
        // None of these has a fixed keystroke, so each note says what the key is
        // for and then what it becomes in each target. The tables themselves are
        // in tmux_context.c, with the test beside it.
        TC_UP: {
            label: '↑',
            desc: 'Up',
            note: 'A line up in all four targets. An arrow rather than k because an arrow is ' +
                'harmless at a Claude prompt, where a letter would be typed into the message.',
        },
        TC_DOWN: {
            label: '↓',
            desc: 'Down',
            note: 'A line down, the mirror of Up. Word and Line make it a bigger step, and how ' +
                'much bigger depends on the target.',
        },
        TC_LEFT: {
            label: '←',
            desc: 'Back one unit',
            note: 'A character back in copy mode, the previous prompt in the viewer, the ' +
                'previous hunk in hunk. Dead at a Claude prompt, where there is nothing to jump ' +
                'between and the key would be typed into the message.',
        },
        TC_RGHT: {
            label: '→',
            desc: 'Forward one unit',
            note: 'The mirror of Back: a character, the next prompt, or the next hunk. Dead at ' +
                'a Claude prompt.',
        },
        TC_SRCH: {
            label: '/',
            desc: 'Search',
            note: 'Starts a search in copy mode, the viewer and hunk, all three of which read ' +
                'it the same way. Sends nothing at a Claude prompt, where it would type a slash ' +
                'into the message.',
        },
        TC_NEXT: {
            label: 'n',
            desc: 'Next match',
            note: 'n — the next match of the last search. Suppressed at a Claude prompt.',
        },
        TC_PREV: {
            label: 'N',
            desc: 'Previous match',
            note: 'N — the previous match. Suppressed at a Claude prompt like n.',
        },
        TC_MARK: {
            label: 'Mark',
            desc: 'Begin a selection at the cursor',
            note: 'Space, which starts a selection where the cursor is and extends by character ' +
                'as you move. Copy mode only — everywhere else a bare Space would go into the ' +
                'program, which is why it resolves like every other key here rather than sitting ' +
                'on the layer as a plain keycode. Mark then move then Copy is how you take part ' +
                'of a line; Select is the whole-line version.',
        },
        TC_SEL: {
            label: 'Select',
            desc: 'Select whole lines',
            note: 'V, which selects by line rather than by character and extends a line at a ' +
                'time. Copy mode only, like Mark.',
        },
        TC_COPY: {
            label: 'Copy',
            desc: 'Yank the selection and leave',
            note: 'Enter, which in copy-mode-vi is copy-pipe-and-cancel, so copy mode closes ' +
                'itself and the keyboard drops to Pane with it. Copy mode only — in the viewer ' +
                'or at a Claude prompt it does nothing, and the mode does not move either.',
        },
        TC_PSTE: {
            label: 'Paste',
            desc: 'Paste the buffer and leave tmux mode',
            note: 'Leaves copy mode, pastes the buffer, and turns tmux mode off — pasting is ' +
                'taken to be the last thing you wanted from tmux. Copy mode only, like Copy.',
        },
        TC_LEAVE: {
            label: 'Leave',
            desc: 'Undo what the target has open',
            note: 'Undoes whatever the target has done, without leaving Within: clears the ' +
                "selection in copy mode, closes the viewer in Claude's. At a Claude prompt there " +
                'is nothing open and it sends nothing — never Escape, which would interrupt the ' +
                'running turn. Nothing in hunk either, which drives itself. Closing the viewer lands back on the prompt, which is still a ' +
                'Claude pane, so the OLED goes from TRSC to CLAUDE rather than out of Within.',
        },
        TC_BKGD: {
            label: 'Bkgd',
            desc: 'Send the running agent to the background',
            note: "Claude's Ctrl-X Ctrl-B, which sends the running agent to the background. On " +
                'both Claude targets and nowhere else. It goes out ' +
                "as a tmux key rather than as the chord itself, because Ctrl-B is tmux's default " +
                'prefix: typed directly, tmux would swallow the second half and take the next ' +
                'key as a prefix command. The tmux binding is send-keys, which writes into the ' +
                "pane past tmux's own key tables.",
        },
        TC_WORD: {
            hold: { label: 'Word', desc: 'Held: a bigger step' },
            note: 'Held, not toggled as it was on the old Copy layer — one unit per thumb, and ' +
                'nothing survives letting go. What the bigger step is depends on the target, so ' +
                'the board above shows the unit rather than the keystroke.',
        },
        TC_LINE: {
            hold: { label: 'Line', desc: 'Held: the biggest step' },
            note: 'Held, like Word. The largest jump the target has: a page, or the file either ' +
                'side of this one in hunk.',
        },

        // --- Tree mode -----------------------------------------------------
        TT_SEL: {
            label: 'Select',
            desc: 'Choose the highlighted item and close the tree',
            note: "Sends Enter, and what that means is up to the tree's own template. The " +
                'keyboard returns to Pane mode because the tree is now closed.',
        },
        TT_KILL: {
            label: 'Kill',
            desc: 'Kill the highlighted session or window — press twice',
            note: 'A bare x, because the tree reads keys itself rather than through the prefix. ' +
                'It kills whatever the cursor is on, session or window, after a second press to ' +
                'confirm; any other key answers no. The tree stays open afterwards. This is the ' +
                'only key on the keyboard that destroys anything, and the tree is deliberately ' +
                'the only place it exists: you have to open the tree and put the cursor on the ' +
                'thing before you can kill it.',
        },

        // --- Copy mode -----------------------------------------------------

        // --- App mode ------------------------------------------------------
    },

    // What the thumb modifiers do to the keys around them.
    //
    // This is the half of the tmux layers you cannot see. Holding Split does not
    // change the arrows, it changes what they send, and keymaps[] has no record
    // of that at all -- it is a switch on tmux_mod inside process_record_user.
    // So it is written here, and the page replays it.
    //
    // Keyed by the modifier's own keycode, then by the keycode of each key it
    // changes; null means the key goes dead while the modifier is live. Which
    // layer a modifier belongs to, and whether it is held or toggled, are both
    // derived -- decorate.mjs finds the keycode on exactly one layer and reads
    // hold vs tap off customKeys above. It fails the build on a modifier that is
    // on no layer or two, on a changed key that is not on the same layer, and on
    // a held modifier that has no entry here at all.
    //
    // `sub` is the second line drawn on the key, so it has to stay short.
    modifiers: {
        TP_RSZE: {
            TP_UP: { sub: 'resize -U 5', desc: 'Grow the pane upwards by five cells' },
            TP_DOWN: { sub: 'resize -D 5', desc: 'Grow the pane downwards by five cells' },
            TP_LEFT: { sub: 'resize -L 5', desc: 'Grow the pane leftwards by five cells' },
            TP_RGHT: { sub: 'resize -R 5', desc: 'Grow the pane rightwards by five cells' },
        },
        TP_SPLT: {
            TP_UP: { sub: 'split -vb', desc: 'split-window -vb — a new pane above this one' },
            TP_DOWN: { sub: 'split -v', desc: 'split-window -v — a new pane below this one' },
            TP_LEFT: { sub: 'split -hb', desc: 'split-window -hb — a new pane to the left' },
            TP_RGHT: { sub: 'split -h', desc: 'split-window -h — a new pane to the right' },
        },
        TP_MOVE: {
            TP_UP: { sub: 'swap ↑', desc: "swap-pane -s '{up-of}' — trade places with the pane above" },
            TP_DOWN: { sub: 'swap ↓', desc: "swap-pane -s '{down-of}' — trade places with the pane below" },
            TP_LEFT: { sub: 'swap ←', desc: "swap-pane -s '{left-of}' — trade places with the pane to the left" },
            TP_RGHT: { sub: 'swap →', desc: "swap-pane -s '{right-of}' — trade places with the pane to the right" },
        },
        TW_MOVE: {
            TW_LEFT: { sub: 'swap -t -1', desc: 'swap-window -d -t -1 — move this window one to the left' },
            TW_RGHT: { sub: 'swap -t +1', desc: 'swap-window -d -t +1 — move this window one to the right' },
            TW_UP: null,
            TW_DOWN: null,
        },
        // Within's two are the one place a modifier cannot be reduced to a
        // keystroke: the same unit is Ctrl-U in Claude's viewer, u in hunk, b in
        // copy mode and Page Up at a Claude prompt. So `sub` names the unit and
        // the description spells out each target.
        TC_WORD: {
            TC_UP: { sub: '½ screen ↑', desc: "Ctrl-U in the viewer, u in hunk, Page Up at a Claude prompt; copy mode leaves Up alone" },
            TC_DOWN: { sub: '½ screen ↓', desc: "Ctrl-D in the viewer, d in hunk, Page Down at a Claude prompt; copy mode leaves Down alone" },
            TC_LEFT: { sub: 'word · annot', desc: 'b — back a word — in copy mode, the previous annotated hunk in hunk; still the previous prompt in the viewer, still dead at a prompt' },
            TC_RGHT: { sub: 'word · annot', desc: 'w — forward a word — in copy mode, the next annotated hunk in hunk; still the next prompt in the viewer, still dead at a prompt' },
        },
        TC_LINE: {
            TC_UP: { sub: 'screen ↑', desc: 'Page Up in copy mode and at a Claude prompt, b — a whole screen back — in the viewer and in hunk' },
            TC_DOWN: { sub: 'screen ↓', desc: 'Page Down in copy mode and at a Claude prompt, Space in the viewer and in hunk' },
            TC_LEFT: { sub: 'line · file', desc: '0 — the start of the line — in copy mode, the previous file in hunk; still the previous prompt in the viewer, still dead at a prompt' },
            TC_RGHT: { sub: 'line · file', desc: '$ — the end of the line — in copy mode, the next file in hunk; still the next prompt in the viewer, still dead at a prompt' },
        },
    },

    // Keyed by "<group>:<key index>", for the things that are about one
    // position rather than one keycode. Plain keycodes on the tree and copy
    // layers need these: they are ordinary keys as far as the keymap is
    // concerned, and only mean something because tmux is reading them.
    keyNotes: {
        'base:32': 'Space cadet: Shift when held, an opening parenthesis when tapped on its own.',
        'base:33': 'Space cadet: Shift when held, a closing parenthesis when tapped on its own.',

        'tree:6': 'Alt with minus folds every item in the tree. The bare - and + keys are only ' +
            'aliases for left and right, which the arrows already cover, so the modified pair ' +
            'is the only reason these two keys are here at all.',
        'tree:8': 'Alt with plus unfolds every item in the tree.',

    },

    chords: [
        {
            title: 'Caps Word',
            keys: [32, 33],
            group: 'base',
            body: 'Press both thumb shifts together to enter Caps Word: the next word types in ' +
                'capitals and it switches itself off at the first space or punctuation. Set by ' +
                'BOTH_SHIFTS_TURNS_ON_CAPS_WORD in config.h — there is no entry for it in the ' +
                'keymap, so it is invisible in the source. The OLED prints CAPS while it is on.',
        },
    ],

    behaviours: [
        {
            title: 'The OLED',
            body: 'Four lines of 21 characters, all of them spoken for. OS and then MAC or ' +
                'LINUX — with LOCK after it while the OS key is overruling the host — then the ' +
                'tmux mode ' +
                'and thumb modifier if tmux mode is on — TREE, WINDOW or PANE, or the Within ' +
                'target it resolved: COPY, TRSC, CLAUDE or HUNK, followed by RESIZE, SPLIT, ' +
                'MOVE, WORD or LINE — then CAPS while Caps Word is ' +
                'active. Blank lines mean neither is on. The tmux layers are toggled rather ' +
                'than held, so this line is the only way to tell which mode you are in. A ' +
                'trailing ~ means no fresh report from the host: every tmux key still works, ' +
                'and only the keys that need to know what is in the pane are missing. A ? means ' +
                'the keyboard is waiting to hear — an intent the host has not finished, or a ' +
                'Ctrl-O whose viewer it has not reported — and a ! is an intent that failed. ' +
                'The third line is for a Claude pane only: what Claude is doing (idle, run, ' +
                'wait) and what it may do without asking (default, plan, accept, auto, bypass, ' +
                'dontask). Neither is guessable from the keyboard, and both change what the next ' +
                'keypress is worth — there is little point stepping through a transcript still ' +
                'being written. Blank for any other program. The fourth is CAPS.',
        },
        {
            title: 'The host reports, the keyboard decides',
            body: 'Over Raw HID the keyboard tells the host what it wants — which mode it is ' +
                'in, which modifier is held, which OS layer it is on — and the host tells the ' +
                'keyboard what it can see: the program in the focused pane, whether the pane ' +
                'is in a tmux mode, and which OS it is actually running. Only that last one ' +
                'is authoritative, which is why the OS key is now an override rather than a ' +
                'switch. Everything else is a report the keyboard feeds into its own state, ' +
                'never a command. The host has to send one every 500 ms, and three missed in ' +
                'a row is what puts the ~ on the OLED. One rule runs the other way: if the ' +
                'host says no tmux mode is open while the keyboard thinks Tree or Copy is, the ' +
                'keyboard believes it and drops to Pane. A pane can close without asking.',
        },
        {
            title: 'App mode drives the program, not tmux',
            body: 'Every other mode sends tmux a function key; App mode sends keys straight to whatever is ' +
                'running in the focused pane. What they mean turns on a state the keymap cannot ' +
                'show, because neither program can be asked: which of Claude, Claude with its ' +
                'transcript viewer open, and hunk you are driving. The App key flips between the ' +
                'two programs and sends nothing, so it is a declaration about what is on screen; ' +
                'Trsc is the one key that changes the screen, by sending Claude the Ctrl-O that ' +
                'opens or closes the viewer, and the only one that can claim the viewer is open. ' +
                "Get the two out of step — close the viewer with Claude's own Escape, say — and " +
                'the OLED is wrong until you press App twice, which declares it closed again. ' +
                'Nothing on the layer sends q, which would quit hunk, or Escape to Claude, which ' +
                'would interrupt the running turn.',
        },
        {
            title: 'Every tmux key is one key',
            body: 'Each tmux action is a single function key from F13 to F24, with Shift, Ctrl ' +
                'or Alt where it needs one, bound in tmux\'s root key table to the command it ' +
                'runs. No prefix, so there is no half-pressed state to get stuck in, and tmux ' +
                'acts the instant the key arrives. The table lives in the host daemon and ' +
                'is generated into tmux.conf from it, so the two cannot drift, and ' +
                'every flag — which directory a new pane opens in, which pane keeps the focus ' +
                'after a swap — lives there rather than in keymap.c. The keymap does not know ' +
                'the prefix at all and has no second path to fall back to: a terminal that will ' +
                'not pass F13 and up through is a terminal this keyboard does not drive.',
        },
        {
            title: 'The thumb modifiers',
            body: 'Pane, Window and App put modifiers on the left thumbs and Copy puts two more ' +
                'there, and they are the part of this keymap you cannot see: they change what ' +
                'the arrows send without changing the arrows. Pane, Window and App hold theirs; ' +
                'Copy toggles its own. Only one can ever be live, and changing mode always ' +
                'clears it, so a Copy toggle cannot survive into Pane. Click one on the board ' +
                'above to hold it down: the keys it changes will say what they send instead, ' +
                'and the ones it kills go dim.',
        },
        {
            title: 'Killing takes two presses, and only from the tree',
            body: 'Kill is the one key here that destroys anything, so it is kept behind as many ' +
                'steps as possible: it exists only in Tree mode, which means opening the tree ' +
                'and moving the cursor onto the session or window first. Then it raises tmux\'s ' +
                'own "(y/n)" prompt rather than killing outright, and since the Tree layer has ' +
                'no y on it, a second press of the same key is what answers. While one is ' +
                'waiting the OLED shows KILL?. Pressing anything else answers no first and then ' +
                'does its own job, so the prompt can never outlive the press that raised it and ' +
                'swallow a later key. Pane and Window mode have no kill at all — closing a pane ' +
                'is still exit in the shell.',
        },
        {
            title: 'Why every layer exists twice',
            body: 'Mac and Linux differ in only three places: the order of the home-row ' +
                'modifiers, the Nav layer bottom row (Mac sends Command+Z/X/C/V/F, Linux sends ' +
                'the dedicated Undo/Cut/Copy/Paste/Find keys), and a Ctrl/Super swap on the ' +
                'Numbers home row. Use the "differs by OS" toggle to see exactly which keys. ' +
                'The six tmux layers are the exception — they are shared, because a function ' +
                'key means the same thing to tmux on both.',
        },
    ],
};

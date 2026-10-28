// Parses keymap.c into a plain 7x36 grid of keycode tokens.
//
// Everything here fails loudly. A reference page that renders a wrong grid
// confidently is worse than one that refuses to build, so every stage asserts
// what it expects and names the layer and key index when it doesn't hold.

import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';

class ParseError extends Error {
    constructor(message) {
        super(message);
        this.name = 'ParseError';
    }
}

function fail(message) {
    throw new ParseError(message);
}

// Strips // and /* */ comments. Only ever call this on a slice we know contains
// no string literals -- oled_task_user is full of PSTR("...") and a naive strip
// over the whole file would mangle it.
function stripComments(text) {
    return text.replace(/\/\*[\s\S]*?\*\//g, ' ').replace(/\/\/[^\n]*/g, '');
}

// Returns the index just past the closer matching the opener at `start`.
function matchDelimiter(text, start, open, close) {
    if (text[start] !== open) {
        fail(`expected '${open}' at offset ${start}, found '${text[start]}'`);
    }
    let depth = 0;
    for (let i = start; i < text.length; i++) {
        if (text[i] === open) depth++;
        else if (text[i] === close) {
            depth--;
            if (depth === 0) return i + 1;
        }
    }
    fail(`unbalanced '${open}' starting at offset ${start}`);
}

// enum layers { _MAC, _LINUX, ... }; -> ['_MAC', '_LINUX', ...]
//
// The comment inside the enum body is why this runs on stripped text. Explicit
// values are rejected rather than guessed at: we rely on positional index to
// resolve LT()/TG() targets, so `_FOO = 9` would silently shift everything.
function parseEnum(source, name) {
    const head = source.indexOf(`enum ${name}`);
    if (head === -1) fail(`could not find 'enum ${name}' in keymap.c`);

    const open = source.indexOf('{', head);
    if (open === -1) fail(`'enum ${name}' has no opening brace`);
    const body = source.slice(open + 1, matchDelimiter(source, open, '{', '}') - 1);

    const members = [];
    for (const raw of body.split(',')) {
        const entry = raw.trim();
        if (entry === '') continue;
        if (entry.includes('=')) {
            fail(
                `'enum ${name}' member '${entry}' has an explicit value; ` +
                `indices would be guesses. Teach parse.mjs about it before continuing.`
            );
        }
        if (!/^[A-Za-z_]\w*$/.test(entry)) {
            fail(`'enum ${name}' member '${entry}' is not a plain identifier`);
        }
        members.push(entry);
    }

    if (members.length === 0) fail(`'enum ${name}' is empty`);
    return members;
}

// keymap.c numbers its custom keycodes from the KB_KEYS list in kb_machine.h,
// so the names come from there, in order.
function parseKbKeys(headerPath) {
    const header = stripComments(readFileSync(headerPath, 'utf8'));
    const start = header.indexOf('#define KB_KEYS(X)');
    if (start === -1) fail(`could not find '#define KB_KEYS(X)' in ${headerPath}`);
    const lines = [];
    for (const line of header.slice(start).split('\n')) {
        lines.push(line);
        if (!line.trimEnd().endsWith('\\')) break;
    }
    const names = [...lines.join('\n').matchAll(/\bX\((\w+)\)/g)].map((m) => m[1]);
    if (names.length === 0) fail('KB_KEYS lists no keys');
    return names;
}

// Object-like #defines only. These are the M_SPC/L_ENT thumb aliases; we keep
// the original token around so the page can show both it and its expansion.
function parseDefines(source) {
    const defines = new Map();
    const pattern = /^[ \t]*#define[ \t]+([A-Za-z_]\w*)[ \t]+(.+)$/gm;
    for (const match of source.matchAll(pattern)) {
        const [, name, value] = match;
        if (defines.has(name)) fail(`#define ${name} appears more than once`);
        defines.set(name, value.trim());
    }
    return defines;
}

// Whole-token replacement to a fixpoint. The depth cap turns a cyclic define
// into an error instead of a hang.
function expandDefines(token, defines) {
    let current = token;
    for (let pass = 0; pass < 8; pass++) {
        const next = current.replace(/[A-Za-z_]\w*/g, (word) =>
            defines.has(word) ? defines.get(word) : word
        );
        if (next === current) return current.replace(/\s+/g, '');
        current = next;
    }
    fail(`#define expansion of '${token}' did not settle; likely a cycle`);
}

// Splits a LAYOUT_*(...) argument list on commas that are at paren depth zero,
// so LCTL_T(KC_S) stays in one piece.
function splitArguments(body, layerName) {
    const tokens = [];
    let depth = 0;
    let start = 0;

    for (let i = 0; i < body.length; i++) {
        const char = body[i];
        if (char === '(') depth++;
        else if (char === ')') {
            depth--;
            if (depth < 0) fail(`[${layerName}] has an unmatched ')'`);
        } else if (char === ',' && depth === 0) {
            tokens.push(body.slice(start, i));
            start = i + 1;
        }
    }
    if (depth !== 0) fail(`[${layerName}] has an unclosed '('`);
    tokens.push(body.slice(start));

    return tokens.map((token, index) => {
        const trimmed = token.trim().replace(/\s+/g, '');
        // Catches a stray trailing comma before the closing paren, which would
        // otherwise produce a 37th empty "keycode".
        if (trimmed === '') fail(`[${layerName}] argument ${index} is empty`);
        return trimmed;
    });
}

/**
 * Reads keymap.c and returns the layer grid plus the raw facts needed to
 * interpret it.
 *
 * @param {string} keymapPath path to keymap.c
 * @param {string[]} knownLayouts layout macro names available in info.json
 */
export function parseKeymap(keymapPath, knownLayouts) {
    const source = stripComments(readFileSync(keymapPath, 'utf8'));

    const layerNames = parseEnum(source, 'layers');
    if (!source.includes('KB_KEYS(KB_KEYCODE)')) fail("'enum custom_keycodes' no longer comes from KB_KEYS");
    const customKeycodes = parseKbKeys(join(dirname(keymapPath), 'kb_machine.h'));
    const defines = parseDefines(source);

    // Slice out just the keymaps[] initialiser, so nothing below it (the
    // process_record_user / oled_task_user bodies) can be mistaken for keycodes.
    const declaration = source.indexOf('keymaps[][MATRIX_ROWS][MATRIX_COLS]');
    if (declaration === -1) {
        fail("could not find the 'keymaps[][MATRIX_ROWS][MATRIX_COLS]' declaration");
    }
    const arrayOpen = source.indexOf('{', declaration);
    if (arrayOpen === -1) fail('keymaps[] has no opening brace');
    const array = source.slice(arrayOpen, matchDelimiter(source, arrayOpen, '{', '}'));

    const layers = [];
    let layoutMacro = null;

    const blockPattern = /\[\s*(\w+)\s*\]\s*=\s*(LAYOUT_\w+)\s*\(/g;
    for (const match of array.matchAll(blockPattern)) {
        const [, layerName, macro] = match;

        if (layoutMacro === null) {
            layoutMacro = macro;
            // The geometry contract. info.json's coordinate list is zipped
            // 1:1 with these arguments, so a different macro means a different
            // key count and a silently wrong board.
            if (!knownLayouts.includes(macro)) {
                fail(
                    `[${layerName}] uses ${macro}, which is not in crkbd/info.json ` +
                    `(known: ${knownLayouts.join(', ')})`
                );
            }
        } else if (macro !== layoutMacro) {
            fail(`[${layerName}] uses ${macro} but earlier layers use ${layoutMacro}`);
        }

        const parenOpen = match.index + match[0].length - 1;
        const body = array.slice(parenOpen + 1, matchDelimiter(array, parenOpen, '(', ')') - 1);
        const tokens = splitArguments(body, layerName);

        layers.push({
            name: layerName,
            tokens,
            expanded: tokens.map((token) => expandDefines(token, defines)),
        });
    }

    if (layers.length === 0) fail('found no [_LAYER] = LAYOUT_*(...) blocks in keymaps[]');

    // Array order must match enum order: TG(_TMUX) and LT(_NAV_MAC, ...) are
    // resolved by index, so a reordered array would point at the wrong layer.
    if (layers.length !== layerNames.length) {
        fail(
            `keymaps[] defines ${layers.length} layers but 'enum layers' has ` +
            `${layerNames.length} (${layerNames.join(', ')})`
        );
    }
    layers.forEach((layer, index) => {
        if (layer.name !== layerNames[index]) {
            fail(
                `keymaps[] entry ${index} is [${layer.name}] but 'enum layers' has ` +
                `${layerNames[index]} at that index`
            );
        }
    });

    // A plain object rather than the Map, so --dump can serialise it.
    return {
        layoutMacro,
        layerNames,
        customKeycodes,
        defines: Object.fromEntries(defines),
        layers,
    };
}

/** Asserts every layer has exactly as many keycodes as the layout has keys. */
export function assertKeyCount(parsed, expected) {
    for (const layer of parsed.layers) {
        if (layer.tokens.length !== expected) {
            fail(
                `[${layer.name}] has ${layer.tokens.length} keycodes but ` +
                `${parsed.layoutMacro} has ${expected} keys`
            );
        }
    }
}

export { ParseError };

#!/usr/bin/env node
// Builds the keymap reference page from keymap.c.
//
//   node build.mjs --dump    print the parsed keymap as JSON, build nothing
//
// All paths are resolved relative to this file, so the working directory
// never matters.

import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

import { parseKeymap, assertKeyCount, ParseError } from './parse.mjs';
import { decorate, DecorateError } from './decorate.mjs';
import annotations from './annotations.mjs';

const here = dirname(fileURLToPath(import.meta.url));

const KEYMAP_C = resolve(here, '../keymap.c');
// What each custom key does, which annotations.mjs describes by hand.
const KB_MACHINE_C = resolve(here, '../kb_machine.c');
const INFO_JSON = resolve(here, '../../../info.json');
const TEMPLATE = resolve(here, 'template.html');
const OUTPUT = resolve(here, 'index.html');
const ANNOTATIONS = resolve(here, 'annotations.mjs');

const DATA_MARKER = '<!--KEYMAP_DATA-->';

/** Reads the crkbd layout table: 36 {matrix, x, y} entries, in keymap.c order. */
function readLayouts() {
    const info = JSON.parse(readFileSync(INFO_JSON, 'utf8'));
    if (!info.layouts) throw new ParseError(`${INFO_JSON} has no 'layouts' section`);
    return info.layouts;
}

/** Short content hash, so you can tell whether the page matches its sources. */
function shortHash(path) {
    return createHash('sha256').update(readFileSync(path)).digest('hex').slice(0, 12);
}

/** Builds the data payload and splices it into the template. */
function render() {
    const layouts = readLayouts();
    const parsed = parseKeymap(KEYMAP_C, Object.keys(layouts));

    const geometry = layouts[parsed.layoutMacro].layout;
    assertKeyCount(parsed, geometry.length);

    const data = {
        layout: parsed.layoutMacro,
        // x/y carry the column stagger and the split gap; h is set on the two
        // inner thumbs. Zipped 1:1 with the keymap.c arguments.
        geometry: geometry.map(({ x, y, w, h }) => ({ x, y, w: w ?? 1, h: h ?? 1 })),
        ...decorate(parsed, annotations),
        // No timestamp: it would break byte-determinism and --check with it,
        // and it would lie about whether the content is current anyway.
        provenance: {
            keymap: shortHash(KEYMAP_C),
            machine: shortHash(KB_MACHINE_C),
            annotations: shortHash(ANNOTATIONS),
        },
    };

    const template = readFileSync(TEMPLATE, 'utf8');
    if (!template.includes(DATA_MARKER)) {
        throw new ParseError(`template.html is missing its ${DATA_MARKER} marker`);
    }

    // Inlined as JSON rather than as generated markup, so this file works over
    // file:// (fetch of a sibling JSON is blocked) and still diffs readably.
    const json = JSON.stringify(data, null, 2).replace(/</g, '\\u003c');
    return { html: template.replace(DATA_MARKER, json), data, parsed, geometry };
}

function main(argv) {
    if (argv.includes('--dump')) {
        const { data } = render();
        process.stdout.write(JSON.stringify(data, null, 2) + '\n');
        return;
    }

    const { html, parsed, geometry } = render();

    if (argv.includes('--check')) {
        // index.html is generated, not committed, so a fresh clone won't have
        // one at all. That is "not built yet", not a crash.
        if (!existsSync(OUTPUT)) {
            console.error(
                'index.html has not been built yet.\n' +
                '  run: node keyboards/crkbd/keymaps/shane935/site/build.mjs'
            );
            process.exit(1);
        }
        const current = readFileSync(OUTPUT, 'utf8');
        if (current !== html) {
            console.error(
                'index.html is stale: keymap.c, kb_machine.c or annotations.mjs changed.\n' +
                '  If kb_machine.c changed what a key does, update annotations.mjs too.\n' +
                "  run: node keyboards/crkbd/keymaps/shane935/site/build.mjs"
            );
            process.exit(1);
        }
        console.log('index.html is up to date');
        return;
    }

    writeFileSync(OUTPUT, html);
    console.log(
        `wrote index.html — ${parsed.layers.length} layers of ${geometry.length} keys ` +
        `(${parsed.layoutMacro}), keymap.c @ ${shortHash(KEYMAP_C)}`
    );
}

try {
    main(process.argv.slice(2));
} catch (error) {
    if (error instanceof ParseError || error instanceof DecorateError) {
        console.error(`${error.name === 'ParseError' ? 'keymap.c' : 'keymap'}: ${error.message}`);
        process.exit(1);
    }
    throw error;
}

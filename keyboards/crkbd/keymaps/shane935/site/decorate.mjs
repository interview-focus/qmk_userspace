// Turns the raw token grid from parse.mjs into the key objects the page renders.
//
// Three kinds of work happen here, and the last two are the reason this file
// exists at all -- they are facts about the keymap that would otherwise have to
// be written out by hand and kept in sync:
//
//   1. decoding  -- LCTL_T(KC_S) becomes tap S / hold Control
//   2. transparency resolution -- _______ becomes what it actually fires
//   3. derivation -- which keys differ between Mac and Linux, how you reach
//      each layer, and where the thumb modifiers and the keys they change sit,
//      computed by diffing and scanning rather than listed

import { lookup, modLabel, MOD_WRAPPERS, LAYER_HEADS } from './keycodes.mjs';

class DecorateError extends Error {
    constructor(message) {
        super(message);
        this.name = 'DecorateError';
    }
}

/** Splits `HEAD(a,b)` into its head and top-level arguments, or null. */
function callParts(token) {
    const open = token.indexOf('(');
    if (open === -1 || !token.endsWith(')')) return null;

    const head = token.slice(0, open);
    const inner = token.slice(open + 1, -1);
    const args = [];
    let depth = 0;
    let start = 0;
    for (let i = 0; i < inner.length; i++) {
        if (inner[i] === '(') depth++;
        else if (inner[i] === ')') depth--;
        else if (inner[i] === ',' && depth === 0) {
            args.push(inner.slice(start, i));
            start = i + 1;
        }
    }
    args.push(inner.slice(start));
    return { head, args: args.map((a) => a.trim()) };
}

/** Short display name for a layer: _NUM_MAC -> NUM. */
function layerLabel(name, groups) {
    for (const group of groups) {
        if (group.mac === name || group.linux === name || group.shared === name) {
            return group.short;
        }
    }
    return name.replace(/^_/, '');
}

/**
 * Decodes one token into {tap, hold, cat, flags}.
 * `where` is a human-readable location used only in error messages.
 */
function decode(token, { os, groups, customKeycodes, customKeys, where }) {
    const plain = lookup(token, os);
    if (plain) return { tap: { label: plain.label, desc: plain.desc }, hold: null, cat: plain.cat, flags: [] };

    if (token === 'XXXXXXX' || token === 'KC_NO') {
        return { tap: null, hold: null, cat: 'blocked', flags: [] };
    }
    if (token === 'SC_LSPO' || token === 'SC_RSPC') {
        const side = token === 'SC_LSPO' ? 'Left' : 'Right';
        const bracket = token === 'SC_LSPO' ? '(' : ')';
        const { label, name } = modLabel('SFT', os);
        return {
            tap: { label: bracket, desc: `${bracket === '(' ? 'Open' : 'Close'} parenthesis` },
            hold: { label, desc: `${side} ${name}` },
            cat: 'space-cadet',
            flags: [],
        };
    }
    if (customKeycodes.includes(token)) {
        // A custom keycode does whatever kb_key in kb_machine.c says it does, and
        // nothing here can read that, so annotations.mjs has to say. Deriving a
        // label from the name would turn TP_RSZE into "TP RSZE" and claim to
        // have understood it.
        const custom = customKeys[token];
        if (!custom) {
            throw new DecorateError(
                `${where}: custom keycode '${token}' has no customKeys entry in annotations.mjs`
            );
        }
        return {
            tap: custom.label ? { label: custom.label, desc: custom.desc } : null,
            hold: custom.hold ?? null,
            cat: 'custom',
            flags: [],
            // Only the keys whose job is to open a layer carry a target; the
            // ones that merely land back on Pane afterwards would otherwise
            // show up as ways in.
            ...(custom.target ? { target: custom.target } : {}),
        };
    }

    const call = callParts(token);
    if (!call) {
        throw new DecorateError(
            `${where}: unknown keycode '${token}'. Add it to keycodes.mjs before rebuilding.`
        );
    }
    const { head, args } = call;

    // Mod-taps: LCTL_T(KC_S), RGUI_T(KC_J), ...
    const modTap = head.match(/^([LR])(CTL|ALT|GUI|SFT)_T$/);
    if (modTap) {
        const [, side, mod] = modTap;
        const inner = decode(args[0], { os, groups, customKeycodes, where });
        const { label, name } = modLabel(mod, os);
        return {
            tap: inner.tap,
            hold: { label, desc: `${side === 'L' ? 'Left' : 'Right'} ${name}` },
            cat: 'mod-tap',
            flags: [],
        };
    }

    // Mod wrappers: G(KC_Z), C(KC_TAB), ...
    if (MOD_WRAPPERS[head] && args.length === 1) {
        const inner = decode(args[0], { os, groups, customKeycodes, where });
        const { label, name } = modLabel(MOD_WRAPPERS[head], os);
        const innerLabel = inner.tap ? inner.tap.label : args[0];
        return {
            tap: { label: `${label}${innerLabel}`, desc: `${name} + ${innerLabel}` },
            hold: null,
            cat: 'mod-combo',
            flags: [],
        };
    }

    // Layer heads: LT(_NAV_MAC, KC_SPC), TG(_TMUX), ...
    if (LAYER_HEADS[head]) {
        const target = layerLabel(args[0], groups);
        if (head === 'LT') {
            const inner = decode(args[1], { os, groups, customKeycodes, where });
            return {
                tap: inner.tap,
                hold: { label: target, desc: `${target} layer` },
                cat: 'layer-tap',
                flags: [],
                target: args[0],
            };
        }
        if (head === 'TG' || head === 'TO' || head === 'TT' || head === 'OSL') {
            return {
                tap: { label: target, desc: LAYER_HEADS[head].replace('{layer}', `the ${target} layer`) },
                hold: null,
                cat: 'layer-toggle',
                flags: [],
                target: args[0],
            };
        }
        // MO
        return {
            tap: null,
            hold: { label: target, desc: `${target} layer` },
            cat: 'layer-toggle',
            flags: [],
            target: args[0],
        };
    }

    throw new DecorateError(
        `${where}: unknown keycode head '${head}' in '${token}'. Teach decorate.mjs about it.`
    );
}

/** Decodes a whole layer, leaving _______ for resolveTransparent to fill in. */
function decodeLayer(layer, context) {
    return layer.expanded.map((token, index) => {
        const where = `[${layer.name}] key ${index}`;
        if (token === '_______' || token === 'KC_TRNS') {
            // No expansion to show: resolveTransparent fills in what it fires.
            return { i: index, src: layer.tokens[index], expanded: null, cat: 'transparent' };
        }
        const decoded = decode(token, { ...context, where });
        return {
            i: index,
            src: layer.tokens[index],
            expanded: token === layer.tokens[index] ? null : token,
            ...decoded,
        };
    });
}

/**
 * Walks down one stack of layers and returns the first that actually maps
 * `index`, or null if the whole stack is transparent there.
 */
function resolveThrough(chain, index, byLayer) {
    for (const layer of chain) {
        const key = byLayer[layer][index];
        if (key.cat !== 'transparent') return { layer, key };
    }
    return null;
}

/**
 * Fills in a transparent key from the layers below it.
 *
 * Showing a literal KC_TRNS would under-report twice over: the two number-layer
 * thumbs and the two nav-layer thumbs are transparent precisely so the layer
 * key you are already holding keeps working, and the tmux mode layers leave the
 * mode keys transparent so the ones on _TMUX show through on all four.
 *
 * `chains` is one stack per OS, each ordered highest layer first. A shared
 * layer gets both, and must resolve to the same thing down either.
 */
function resolveTransparent(keys, chains, byLayer) {
    return keys.map((key) => {
        if (key.cat !== 'transparent') return key;

        const resolutions = chains.map((chain) => resolveThrough(chain, key.i, byLayer));
        if (resolutions.some((r) => r === null)) {
            throw new DecorateError(
                `key ${key.i} is transparent all the way down (${chains[0].join(' -> ')}), ` +
                'so nothing says what it fires'
            );
        }

        const distinct = new Set(resolutions.map((r) => JSON.stringify([r.key.tap, r.key.hold])));
        if (distinct.size !== 1) {
            throw new DecorateError(
                `key ${key.i} is transparent on a shared layer but resolves differently per OS: ` +
                resolutions.map((r) => `${r.layer}=${r.key.src}`).join(', ')
            );
        }

        const from = resolutions[0];
        return {
            ...key,
            tap: from.key.tap,
            hold: from.key.hold,
            resolvedFrom: { layer: from.layer, src: from.key.src, cat: from.key.cat },
        };
    });
}

/**
 * Scans every layer for keys that lead to any of `targets`.
 *
 * Both halves of an OS pair are targets at once, because the two halves of the
 * pair that reaches them hold the same key in the same place and differ only in
 * which half they point at: M_SPC opens _NAV_MAC and L_SPC opens _NAV_LINUX, off
 * the same thumb. Those collapse into one entry carrying both layer names, or
 * the page offers you "hold Space on the BASE layer" twice.
 */
function accessPaths(layers, targets, decodedByLayer, groups) {
    const paths = [];
    for (const layer of layers) {
        const fromLabel = layerLabel(layer.name, groups);
        decodedByLayer[layer.name].forEach((key) => {
            if (!key.target || !targets.includes(key.target)) return;
            const via = key.cat === 'layer-tap' ? 'hold' : 'tap';
            const label = key.tap?.label ?? key.hold?.label ?? null;

            const same = paths.find(
                (p) => p.key === key.i && p.via === via && p.label === label && p.fromLabel === fromLabel
            );
            if (same) {
                same.from.push(layer.name);
                return;
            }
            paths.push({ from: [layer.name], fromLabel, key: key.i, via, label });
        });
    }
    return paths;
}

/**
 * Builds the full data payload for the page.
 *
 * @param {object} parsed output of parseKeymap
 * @param {object} annotations hand-written prose and group definitions
 */
export function decorate(parsed, annotations) {
    const { groups, customKeys } = annotations;
    const roles = layerRoles(groups);
    const byLayer = {};

    // Pass one: decode every layer under its own OS, so G() reads as Command on
    // Mac and Super on Linux.
    for (const layer of parsed.layers) {
        const os = roles.get(layer.name);
        if (!os) {
            throw new DecorateError(
                `layer ${layer.name} is not claimed by any group in annotations.mjs`
            );
        }
        byLayer[layer.name] = decodeLayer(layer, {
            os,
            groups,
            customKeycodes: parsed.customKeycodes,
            customKeys,
        });
    }

    // Pass two: resolve transparency down each group's stack and then onto the
    // base layer. The under-layers have no transparency of their own, so the
    // order groups are visited in cannot matter.
    const base = groups.find((g) => g.id === annotations.baseGroup);
    if (!base) throw new DecorateError(`baseGroup '${annotations.baseGroup}' is not a group`);

    for (const group of groups) {
        if (group.id === base.id) continue;
        const under = [...(group.under ?? [])].reverse();
        for (const [os, layerName] of Object.entries(variantsOf(group))) {
            // A shared layer is live under either OS, so it has to agree down
            // both stacks; an OS-specific one only ever sees its own base.
            const chains = group.shared
                ? [[...under, base.mac], [...under, base.linux]]
                : [[...under, base[os]]];
            byLayer[layerName] = resolveTransparent(byLayer[layerName], chains, byLayer);
        }
    }

    // Pass three: derive what would otherwise be hand-maintained.
    const outGroups = groups.map((group) => {
        const variants = variantsOf(group);
        const layerNames = [...new Set(Object.values(variants))];

        const osDiff = group.shared
            ? []
            : parsed.layers
                  .find((l) => l.name === group.mac)
                  .expanded.map((token, i) =>
                      token === parsed.layers.find((l) => l.name === group.linux).expanded[i] ? null : i
                  )
                  .filter((i) => i !== null);

        const access = accessPaths(parsed.layers, layerNames, byLayer, groups);
        if (group.id !== base.id && access.length === 0) {
            throw new DecorateError(
                `no key anywhere reaches ${layerNames.join('/')} -- the ${group.id} layer is unreachable`
            );
        }

        return {
            id: group.id,
            title: group.title,
            short: group.short,
            shared: Boolean(group.shared),
            prose: annotations.groupProse[group.id] ?? null,
            banner: annotations.groupBanner?.[group.id] ?? null,
            osDiff,
            access,
            keys: Object.fromEntries(
                Object.entries(variants).map(([os, name]) => [
                    os,
                    { layer: name, keys: attachNotes(byLayer[name], group.id, annotations) },
                ])
            ),
        };
    });

    attachModifiers(outGroups, annotations);
    assertBehaviourCoverage(outGroups, annotations, parsed);
    return { groups: outGroups, meta: annotations.meta, chords: annotations.chords, behaviours: annotations.behaviours };
}

function variantsOf(group) {
    return group.shared ? { mac: group.shared, linux: group.shared } : { mac: group.mac, linux: group.linux };
}

/**
 * Every layer name annotations.mjs accounts for, and which OS it reads as.
 *
 * A layer earns its place either by being a group's own layer or by being
 * stacked under one -- _TMUX is only ever the latter, because tmux mode is
 * never on without exactly one mode layer above it.
 */
function layerRoles(groups) {
    const roles = new Map();
    for (const group of groups) {
        if (group.shared) roles.set(group.shared, 'mac');
        else {
            roles.set(group.mac, 'mac');
            roles.set(group.linux, 'linux');
        }
    }
    // Second pass, so an under-layer can never shadow a group's own layer.
    for (const group of groups) {
        for (const name of group.under ?? []) {
            if (!roles.has(name)) roles.set(name, 'mac');
        }
    }
    return roles;
}

/**
 * A note follows the behaviour, not the position: TM_TREE means the same thing
 * on all five tmux layers, so it is described once in customKeys and picked up
 * wherever it appears -- including where a mode layer shows it through a
 * transparent key. keyNotes stays for the things that are about one position.
 */
function attachNotes(keys, groupId, annotations) {
    return keys.map((key) => {
        const source = key.resolvedFrom?.src ?? key.src;
        return {
            ...key,
            note:
                annotations.keyNotes[`${groupId}:${key.i}`] ??
                annotations.customKeys[source]?.note ??
                null,
        };
    });
}

/** Every position on `group` holding `token`, by key index. */
function positionsOf(group, token) {
    // The two OS variants of a group sit their keys in the same places, so
    // either one answers the question; a shared layer only has the one.
    return group.keys.mac.keys
        .filter((key) => (key.resolvedFrom?.src ?? key.src) === token)
        .map((key) => key.i);
}

/**
 * Turns annotations.modifiers into positions the page can draw.
 *
 * Nothing about placement is written down twice: which layer a modifier belongs
 * to is found by looking for its keycode, and held versus toggled comes from
 * whether its customKeys entry has a `hold`. Move a modifier in keymap.c and
 * this follows it; take one off a layer and the build says so.
 */
function attachModifiers(outGroups, annotations) {
    const modifiers = annotations.modifiers ?? {};

    // A key you hold changes what other keys send -- that is the only reason to
    // hold one -- and that effect is invisible in keymaps[]. So a held key with
    // nothing written about it is the exact gap this section exists to close.
    for (const [token, custom] of Object.entries(annotations.customKeys)) {
        if (custom.hold && !modifiers[token]) {
            throw new DecorateError(
                `${token} is held rather than tapped, so it changes what other keys send, and ` +
                'nothing in keymaps[] records that. Add it to `modifiers` in annotations.mjs.'
            );
        }
    }

    for (const group of outGroups) group.mods = [];

    for (const [token, changes] of Object.entries(modifiers)) {
        const custom = annotations.customKeys[token];
        if (!custom) {
            throw new DecorateError(
                `annotations.mjs has a modifiers entry for '${token}', which is not a customKeys entry`
            );
        }

        const homes = outGroups.filter((group) => positionsOf(group, token).length > 0);
        if (homes.length !== 1) {
            throw new DecorateError(
                `modifier '${token}' is on ${homes.length === 0 ? 'no layer' : homes.map((g) => g.id).join(' and ')}` +
                ' -- it has to be on exactly one, because the page arms it by clicking it there'
            );
        }
        const group = homes[0];
        const at = positionsOf(group, token);
        if (at.length !== 1) {
            throw new DecorateError(`modifier '${token}' is on the ${group.id} layer ${at.length} times`);
        }

        const overrides = {};
        for (const [target, change] of Object.entries(changes)) {
            const where = positionsOf(group, target);
            if (where.length !== 1) {
                throw new DecorateError(
                    `'${token}' says it changes '${target}', which is on the ${group.id} layer ` +
                    `${where.length} times -- it has to be there exactly once`
                );
            }
            overrides[where[0]] = change;
        }

        group.mods.push({
            key: at[0],
            label: (custom.hold ?? custom).label,
            via: custom.hold ? 'hold' : 'toggle',
            overrides,
        });
    }
}

/**
 * The page must never show a key whose behaviour lives in the C without saying
 * what that behaviour is. Custom keycodes, layer toggles and the space-cadet
 * shifts all qualify, so the build refuses if any of them lack a note.
 */
function assertBehaviourCoverage(outGroups, annotations, parsed) {
    const needsNote = new Set(['custom', 'layer-toggle', 'space-cadet']);
    const missing = [];

    for (const group of outGroups) {
        for (const [os, variant] of Object.entries(group.keys)) {
            for (const key of variant.keys) {
                if (needsNote.has(key.cat) && !key.note) {
                    missing.push(`${group.id}:${key.i} (${key.src}, on ${variant.layer})`);
                }
            }
        }
    }
    if (missing.length) {
        throw new DecorateError(
            'these keys do what they do because of code in keymap.c, so they need a note in ' +
            'annotations.mjs -- customKeys for a custom keycode, keyNotes otherwise:\n  ' +
            [...new Set(missing)].join('\n  ')
        );
    }

    // And the reverse: a note pointing at a key that no longer exists is a sign
    // the keymap moved under us.
    const valid = new Set(
        outGroups.flatMap((g) =>
            Object.values(g.keys).flatMap((v) => v.keys.map((k) => `${g.id}:${k.i}`))
        )
    );
    for (const ref of Object.keys(annotations.keyNotes)) {
        if (!valid.has(ref)) {
            throw new DecorateError(`annotations.mjs has a keyNote for '${ref}', which is not a key`);
        }
    }
    for (const token of Object.keys(annotations.customKeys)) {
        if (!parsed.customKeycodes.includes(token)) {
            throw new DecorateError(
                `annotations.mjs describes custom keycode '${token}', which keymap.c no longer has`
            );
        }
    }
}

export { DecorateError };

// Compares what read_contract got out of the daemon's output.bin with the
// fields the daemon says went in, from its input.json.
//
//   node check_contract.mjs <input.json> <read_contract output>

import { readFileSync } from 'node:fs';
import { isDeepStrictEqual } from 'node:util';

const [inputPath, readPath] = process.argv.slice(2);
const want = JSON.parse(readFileSync(inputPath, 'utf8'));
const got = JSON.parse(readFileSync(readPath, 'utf8'));

const failures = [];
if (want.version !== got.version) failures.push(`version: daemon ${want.version}, keyboard ${got.version}`);
if (want.reportSize !== got.reportSize) failures.push(`reportSize: daemon ${want.reportSize}, keyboard ${got.reportSize}`);
if (want.messages.length !== got.messages.length) {
    failures.push(`${want.messages.length} cases in input.json, ${got.messages.length} reports in output.bin`);
}
want.messages.forEach((m, i) => {
    const read = got.messages[i];
    if (read === null) {
        failures.push(`${m.name}: rejected by kb_context_parse`);
    } else if (read !== undefined && !isDeepStrictEqual(read, m.fields)) {
        failures.push(`${m.name}: read ${JSON.stringify(read)}, want ${JSON.stringify(m.fields)}`);
    }
});

for (const f of failures) console.log(`FAIL to-keyboard ${f}`);
process.exitCode = failures.length ? 1 : 0;

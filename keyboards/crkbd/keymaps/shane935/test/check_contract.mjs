// Compares what read_contract got out of the daemon's output.bin with the
// fields the daemon says went in, from its input.json.
//
//   node check_contract.mjs <daemon contract dir> <read_contract output> <verified file> [record]
//
// The verified file records which daemon contract last passed: its sha256 and
// the daemon commit it came from. A different hash is only a warning, since the
// cases passing is what matters. `record` rewrites it, and only on a pass.

import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { isDeepStrictEqual } from 'node:util';

const [dir, readPath, verifiedPath, mode] = process.argv.slice(2);
const inputBytes = readFileSync(`${dir}/input.json`);
const want = JSON.parse(inputBytes);
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

const sha256 = createHash('sha256').update(inputBytes).update(readFileSync(`${dir}/output.bin`)).digest('hex');
const verified = existsSync(verifiedPath) ? JSON.parse(readFileSync(verifiedPath, 'utf8')) : null;

if (failures.length === 0 && mode === 'record') {
    const daemonCommit = execFileSync('git', ['-C', dir, 'describe', '--always', '--dirty', '--abbrev=40'], { encoding: 'utf8' }).trim();
    writeFileSync(verifiedPath, `${JSON.stringify({ daemonCommit, contractSha256: sha256 }, null, 2)}\n`);
    console.log(`contract: to-keyboard verified against daemon ${daemonCommit}`);
} else if (failures.length === 0 && verified?.contractSha256 !== sha256) {
    const since = verified ? `changed since it last passed against ${verified.daemonCommit}` : 'has not been recorded as passing';
    console.log(`WARN to-keyboard: the daemon's contract ${since}; run test/run.sh record to note that it passes now`);
}
process.exitCode = failures.length ? 1 : 0;

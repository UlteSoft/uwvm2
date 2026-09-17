// Mechanical source inventory; --write is ONLY for an audited vendor update.
// Ordinary builds verify the resulting manifest, never silently regenerate it.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
const root = path.dirname(fileURLToPath(import.meta.url));
const records = [];
const materializeLinks = process.argv.includes('--materialize-links');
function walk(relative) {
  const file = path.join(root, relative);
  const stat = fs.lstatSync(file);
  if (stat.isDirectory()) {
    for (const name of fs.readdirSync(file).sort()) walk(path.posix.join(relative, name));
  } else {
    if (!(stat.isFile() || (stat.isSymbolicLink() && fs.statSync(file).isFile()
        && fs.realpathSync(file).startsWith(root + path.sep)))) {
      throw new Error(`Unexpected/escaping source: ${relative}`);
    }
    const content = fs.readFileSync(file);
    if (stat.isSymbolicLink() && materializeLinks) {
      // Mechanical archive normalization: Windows Git checkouts often cannot
      // create symlinks. Replace only verified in-tree file links, preserving
      // their exact target bytes and mode. No external target can be modified.
      const mode = fs.statSync(file).mode;
      fs.unlinkSync(file);
      fs.writeFileSync(file, content, {mode});
      console.log(`Materialized ${relative}`);
    }
    const digest = crypto.createHash('sha256').update(content).digest('hex');
    records.push(`${digest}  ${relative}\n`);
  }
}
for (const name of ['LICENSE.TXT', 'cmake', 'libc', 'llvm', 'third-party']) walk(name);
const text = records.sort().join('');
const manifest = path.join(root, 'sources.sha256');
if (process.argv.includes('--write')) fs.writeFileSync(manifest, text);
else if (fs.readFileSync(manifest, 'utf8') !== text) throw new Error('Bundled LLVM source inventory mismatch');
console.log(`${records.length} retained files: ${crypto.createHash('sha256').update(text).digest('hex')}`);

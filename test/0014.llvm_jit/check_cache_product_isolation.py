#!/usr/bin/env python3
"""Check separately compiled ordinary/ROS cache_product_isolation fixtures.

Uses a fresh directory and identical embedder contexts, not CLI defaults.
Objects are inert test bytes. A signed cache is an integrity check, not an
access-control boundary against an attacker who can replace files as this user.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--full', type=Path, required=True)
parser.add_argument('--ros', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
args = parser.parse_args()
args.out.mkdir(parents=True, exist_ok=False)
binaries = {'uwvm2': args.full.resolve(), 'uwvm2ros': args.ros.resolve()}
rows = []

def run(product, *arguments):
    command = [str(binaries[product]), *map(str, arguments)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=60)
    row = dict(command=command, returncode=result.returncode,
               stdout=result.stdout, stderr=result.stderr)
    rows.append(row)
    (args.out/'results.json').write_text(json.dumps(rows, indent=2)+'\n')
    if result.returncode:
        raise RuntimeError(row)
    return dict(line.split('=', 1) for line in result.stdout.splitlines() if '=' in line)

defaults = {p: run(p) for p in binaries}
for product, values in defaults.items():
    assert values['product'] == product and values['version'] == '5'
    assert product in values['default']
    abi = bytes.fromhex(values['abi-hex'])
    assert (product+'-runtime-abi-v12').encode() in abi
    policy = 'unique-temporary-labels-v1' if product == 'uwvm2' else 'symbol-entry-identity-v1'
    assert b'llvm-elf-local-symbols' in abi and policy.encode() in abi
    assert b'llvm-mips-call-relocations' in abi and b'full-width-noabicalls-c-abi-v2' in abi
assert defaults['uwvm2']['default'] != defaults['uwvm2ros']['default']

for mode in ('signed', 'unsigned'):
    # The caller-supplied root/key/ABI are identical: storage must namespace
    # itself, including callers that bypass environment/CLI context creation.
    shared = args.out/mode/'shared'
    own = {p: Path(run(p, 'write', shared, mode)['path']) for p in binaries}
    assert own['uwvm2'] != own['uwvm2ros']
    for product, path in own.items():
        assert run(product, 'policy-v1', args.out/mode/product/'policy-v1', mode)['status'] == 'context-mismatch'
        assert path.relative_to(shared).parts[0] == product
        assert path.name.startswith(product+'-') and path.suffix == '.uwvm-ljc'
        assert path.is_file()
        foreign = own['uwvm2ros' if product == 'uwvm2' else 'uwvm2']
        for mutation, status in (('none', 'invalid-magic'), ('own-magic', 'context-mismatch')):
            root = args.out/mode/product/mutation
            run(product, 'load-foreign', root, mode, foreign, mutation, status)
        run(product, 'load-foreign', args.out/mode/product/'v4', mode,
            path, 'version4', 'unsupported-version')
        if mode == 'signed':
            run(product, 'load-foreign', args.out/mode/product/'payload', mode,
                path, 'payload', 'signature-mismatch')

summary = dict(passed=True, invocations=len(rows), format_version=5,
               binary_sha256={p: hashlib.sha256(b.read_bytes()).hexdigest() for p, b in binaries.items()},
               scope='serializer, path, context and signature; no native object execution')
(args.out/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
print(json.dumps(summary))

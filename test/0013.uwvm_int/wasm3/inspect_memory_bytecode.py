#!/usr/bin/env python3
"""Inspect the real ring stream used by the memory performance harness (ELF64)."""
import argparse
import json
import os
from pathlib import Path
import re
import struct
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('binary', type=Path)
p.add_argument('fixtures', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--llvm', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
a.output.mkdir(parents=True, exist_ok=False)
symbols = {}
for line in subprocess.check_output([str(a.llvm / 'llvm-nm'), '-C', '-S', '--defined-only', str(a.binary)], text=True).splitlines():
    fields = line.split(maxsplit=3)
    if len(fields) == 4:
        address, size, kind, name = fields
        if kind.lower() in ('t', 'w'):
            symbols[int(address, 16)] = (int(size, 16), name)
main = next(address for address, (_, name) in symbols.items() if name == 'main')
rows = []
used = set()
for fixture in sorted(a.fixtures.glob('*.wasm')):
    dump = a.output / (fixture.stem + '.bytecode')
    env = dict(os.environ, UWVM_TEST_DUMP_BYTECODE=str(dump.resolve()))
    run = subprocess.run([str(a.binary.resolve()), str(fixture.resolve()), '101', '1'], env=env, capture_output=True, text=True)
    if run.returncode:
        raise RuntimeError(run.stderr)
    bias = int(re.search(r'main=([0-9a-f]+)', run.stderr)[1], 16) - main
    data = dump.read_bytes()
    operations = []
    for offset in range(len(data) - 7):
        address = struct.unpack_from('<Q', data, offset)[0] - bias
        if address in symbols and '::optable::' in symbols[address][1]:
            operations.append(dict(offset=offset, symbol=symbols[address][1], address=address))
            used.add(address)
    rows.append(dict(fixture=fixture.stem, bytes=len(data), operations=operations))
    print(f'{fixture.stem}: {len(data)} bytes, {len(operations)} dispatched handlers')
for address in sorted(used):
    size, name = symbols[address]
    dis = subprocess.check_output([str(a.llvm / 'llvm-objdump'), '-d', '--no-show-raw-insn',
        f'--start-address={address}', f'--stop-address={address + size}', str(a.binary)], text=True)
    (a.output / f'{address:x}.s').write_text(dis)
(a.output / 'streams.json').write_text(json.dumps(rows, indent=2) + '\n')

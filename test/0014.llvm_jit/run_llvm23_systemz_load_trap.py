#!/usr/bin/env python3
"""Verify real SystemZ load/trap codegen and execute the alias/cursor regression."""
import argparse
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('output', type=Path)
p.add_argument('--llvm', type=Path, required=True)
p.add_argument('--clang', type=Path, required=True)
p.add_argument('--deps', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
root = Path(__file__).resolve().parents[2]
subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
fixture = root / 'test/0014.llvm_jit/fixtures/llvm23-systemz-load-trap'
triple = 's390x-linux-gnu'
sysroot = a.deps / 'usr' / triple
gcc = a.deps / 'usr/lib/gcc-cross' / triple / '15'
compiler = [str(a.clang), f'--target={triple}', f'--sysroot={a.deps}', f'--gcc-install-dir={gcc}',
            '-idirafter', str(sysroot / 'include'), '-O2', '-fuse-ld=lld']
codegen = [str(a.llvm / 'llc'), '-mtriple=' + triple, '-mcpu=z14', '-O2', '-verify-machineinstrs']
rows = []
for phase, command in [
    ('assembly', [*codegen, str(fixture) + '.ll', '-o', str(a.output / 'regression.s')]),
    ('object', [*codegen, '-filetype=obj', str(fixture) + '.ll', '-o', str(a.output / 'regression.o')]),
    ('oracle-ir', [*compiler, '-S', '-emit-llvm', str(fixture) + '.c', '-o', str(a.output / 'oracle.ll')]),
    ('oracle-object', [*codegen, '-filetype=obj', str(a.output / 'oracle.ll'), '-o', str(a.output / 'oracle.o')]),
    ('link', [*compiler, str(a.output / 'regression.o'), str(a.output / 'oracle.o'),
              '-L' + str(sysroot / 'lib'), '-o', str(a.output / 'regression.test')]),
    ('execute', [str(a.deps / 'usr/bin/qemu-s390x'), '-U', 'LD_LIBRARY_PATH', '-L', str(sysroot),
                 str(a.output / 'regression.test')]),
]:
    run = subprocess.run(command, capture_output=True, timeout=120)
    (a.output / (phase + '.log')).write_bytes(run.stdout + run.stderr)
    rows.append(dict(phase=phase, command=command, exit=run.returncode))
    (a.output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
    if run.returncode:
        raise RuntimeError(f'{phase}: {run.stderr.decode(errors="replace")}')
assembly = (a.output / 'regression.s').read_text()
for name, should_fuse in [('load_trap_with_intervening_store', False),
                          ('load_before_alias_store', False), ('adjacent_load_and_trap', True)]:
    body = assembly.split(name + ':', 1)[1].split('.size', 1)[0]
    if ('\tlgat\t' in body) != should_fuse:
        raise RuntimeError(f'{name}: unexpected load/trap fusion')
print('PASS: verified SystemZ machine instructions, safe fusion, 999 alias/cursor executions')

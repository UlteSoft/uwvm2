#!/usr/bin/env python3
"""Check NEW indexed-memory traps and nested stacks in actual runtime modes.

Assemble only Core 3 multi-memory syntax; enabling all WABT proposals would also
enable newer compact-import encodings that are outside this task's Core 3 scope.
The third memory is deliberately larger so a wrong memory-index selection can
turn the intended bounds trap into a successful access.
"""
import argparse
import json
from pathlib import Path
import re
import resource
import subprocess
from run_wasm3_multi_memory import configurations

OPERATIONS = {
    'scalar-load': 'i32.const 65535 i32.load $small drop',
    'scalar-store': 'i32.const 65535 i32.const 42 i32.store $small',
    'simd-load': 'i32.const 65530 v128.load $small drop',
    'simd-store-lane': 'i32.const 65535 v128.const i32x4 0 0 0 0 v128.store32_lane $small 0',
    'copy-source': 'i32.const 0 i32.const 65535 i32.const 2 memory.copy $large $small',
    'copy-destination': 'i32.const 65535 i32.const 0 i32.const 2 memory.copy $small $large',
    'fill': 'i32.const 65535 i32.const 42 i32.const 2 memory.fill $small',
    'init': 'i32.const 65535 i32.const 0 i32.const 2 memory.init $small $bytes',
}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('output', type=Path)
    p.add_argument('--uwvm', type=Path, required=True)
    p.add_argument('--wat2wasm', type=Path, required=True)
    p.add_argument('--wasmtime', type=Path)
    p.add_argument('--ros', action='store_true')
    p.add_argument('--configuration', help='run a single runtime configuration')
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    fixtures = []
    for name, operation in OPERATIONS.items():
        wat = a.output / (name + '.wat')
        wasm = wat.with_suffix('.wasm')
        wat.write_text('''(module
  (memory $unused 3) (memory $small 1) (memory $large 2)
  (data $bytes "abcd")
  (func $leaf %s)
  (func $caller call $leaf)
  (func (export "_start") call $caller))
''' % operation)
        subprocess.run([str(a.wat2wasm), '--enable-multi-memory', str(wat), '-o', str(wasm)], check=True)
        fixtures.append((name, wasm))
    configs = configurations(a.uwvm, a.ros)
    # Test the explicit no-recording policy too; it must not leak stale frames.
    for name, command in configs[:]:
        if name.startswith('jit-') and name.endswith('-unwind'):
            command = command[:]
            command[command.index('-Rllvm-call-stack') + 1] = 'none'
            configs.append((name.removesuffix('-unwind') + '-none', command))
    if a.wasmtime:
        configs.append(('wasmtime', [str(a.wasmtime), '-C', 'cache=n', '-W', 'multi-memory=y']))
    if a.configuration:
        configs = [c for c in configs if c[0] == a.configuration]
        if not configs:
            p.error('unknown configuration')
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')
    rows = []
    for name, command in configs:
        for operation, fixture in fixtures:
            run = subprocess.run(command + [str(fixture)], capture_output=True, timeout=60)
            out = ansi.sub('', (run.stdout + run.stderr).decode(errors='replace'))
            (a.output / (name + '-' + operation + '.log')).write_text(out)
            stack = [int(n) for n in re.findall(r'func_idx=(\d+)', out)]
            expected = [] if name.endswith('-none') else [0, 1, 2]
            if run.returncode == 0 or not any(s in out.lower() for s in ('out of bounds', 'out-of-bounds', 'out_of_bounds')):
                raise RuntimeError(f'{name}/{operation}: missing memory bounds trap: {run.returncode}\n{out}')
            if name != 'wasmtime' and stack != expected:
                raise RuntimeError(f'{name}/{operation}: expected stack {expected}, got {stack}\n{out}')
            memory_indices = [int(n) for n in re.findall(r'memory\[(\d+)\] page protection fault', out)]
            if memory_indices and memory_indices != [1]:
                raise RuntimeError(f'{name}/{operation}: fault attributed to wrong memory: {memory_indices}')
            # Native full-guard accesses expose the owning memory index. Software
            # bounds traps need not print a page-fault record at all.
            rows.append(dict(configuration=name, operation=operation, exit=run.returncode,
                             stack=stack, memory_indices=memory_indices, passed=True))
        print(f'PASS {name}: {len(fixtures)} indexed-memory traps and nested call stacks', flush=True)
    (a.output / 'results.json').write_text(json.dumps(dict(configurations=len(configs), new_syntax_traps=len(fixtures), runs=len(rows), cases=rows), indent=2) + '\n')


if __name__ == '__main__':
    main()

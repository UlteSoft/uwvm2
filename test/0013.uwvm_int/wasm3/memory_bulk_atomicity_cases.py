#!/usr/bin/env python3
"""Generate same-instance Core 3 memory.copy/init fault-first fixtures.

The `_start` oracle only checks ordinary successful execution. The companion
native harness observes the same module instance at each deliberate trap;
running a second VM after a trap would not prove that no prefix was written.
"""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    args = parser.parse_args()
    subprocess.run(['bash', str(Path(__file__).resolve().parents[3] /
                               'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.output.mkdir(parents=True, exist_ok=False)
    rows = []
    for memory64 in (False, True):
        address = 'i64' if memory64 else 'i32'
        declaration = 'i64 ' if memory64 else ''
        constant = f'{address}.const'
        for operation in ('copy', 'init'):
            name = f'memory-{operation}-m{64 if memory64 else 32}'
            src_type = address if operation == 'copy' else 'i32'
            length_type = address if operation == 'copy' else 'i32'
            opcode = ('memory.copy $target $source' if operation == 'copy'
                      else 'memory.init $target $passive')
            payload = ('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef' if operation == 'copy'
                       else 'QRSTUVWXYZabcdefghijklmnopqrstuv')
            first_expected = ord(payload[0])
            last_expected = ord(payload[15])
            wat = f'''(module
  (memory $source {declaration}1 2)
  (memory $target {declaration}1 2)
  (data (memory $source) ({constant} 0) "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef")
  (data $passive "QRSTUVWXYZabcdefghijklmnopqrstuv")
  (func $run (export "run") (param $dst {address})
        (param $src {src_type}) (param $len {length_type})
    local.get $dst local.get $src local.get $len {opcode})
  (func (export "_start")
    {constant} 128 {src_type}.const 0 {length_type}.const 16 call $run
    {constant} 128 i32.load8_u $target i32.const {first_expected} i32.ne
    if unreachable end
    {constant} 143 i32.load8_u $target i32.const {last_expected} i32.ne
    if unreachable end
    {constant} 0 i32.load8_u $source i32.const 65 i32.ne
    if unreachable end))
'''
            wat_path = args.output / f'{name}.wat'
            wasm_path = args.output / f'{name}.wasm'
            wat_path.write_text(wat)
            subprocess.run([str(args.wasm_tools), 'parse', str(wat_path),
                            '-o', str(wasm_path)], check=True)
            subprocess.run([str(args.wasm_tools), 'validate', str(wasm_path)],
                           check=True)
            wasm_flags = ['-W', 'multi-memory=y']
            if memory64:
                wasm_flags += ['-W', 'memory64=y']
            # WASI-free `_start` is an ordinary exported test function here.
            subprocess.run([str(args.wasmtime), 'run', *wasm_flags,
                            '--invoke', '_start', str(wasm_path)], check=True)
            edge = 65536
            bad = ([('source-partial', 128, edge - 8, 16),
                    ('destination-partial', edge - 8, 0, 16),
                    ('source-zero-beyond', 128, edge + 1, 0),
                    ('destination-zero-beyond', edge + 1, 0, 0)]
                   if operation == 'copy' else
                   [('data-source-partial', 128, 24, 16),
                    ('destination-partial', edge - 8, 0, 16),
                    ('data-source-zero-beyond', 128, 33, 0),
                    ('destination-zero-beyond', edge + 1, 0, 0)])
            oob_oracle = []
            for scenario, dst, src, length in bad:
                command = [str(args.wasmtime), 'run', *wasm_flags, '--invoke',
                           'run', str(wasm_path), str(dst), str(src), str(length)]
                completed = subprocess.run(command, capture_output=True,
                                           text=True, timeout=10)
                if completed.returncode == 0 or \
                   'out of bounds memory access' not in completed.stderr:
                    raise RuntimeError(f'Wasmtime did not trap {name}/{scenario}: '
                                       f'{completed.returncode} {completed.stderr}')
                oob_oracle.append({'scenario': scenario, 'arguments':
                                   [dst, src, length], 'exit': completed.returncode,
                                   'stderr_sha256': hashlib.sha256(
                                       completed.stderr.encode()).hexdigest()})
            rows.append({'name': name, 'operation': operation,
                         'memory64': memory64, 'wasm': str(wasm_path.resolve()),
                         'wat_sha256': sha256(wat_path),
                         'wasm_sha256': sha256(wasm_path),
                         'wasm_tools_validated': True,
                         'wasmtime_start_executed': True,
                         'wasmtime_oob_oracle': oob_oracle})
    summary = {'cases': rows, 'wasm_tools_sha256': sha256(args.wasm_tools),
               'wasmtime_sha256': sha256(args.wasmtime),
               'generator_sha256': sha256(Path(__file__))}
    (args.output / 'reference.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('PASS 4 memory.copy/init memory32/64 Wasm oracle fixtures')


if __name__ == '__main__':
    main()

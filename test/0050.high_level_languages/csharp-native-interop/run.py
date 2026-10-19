#!/usr/bin/env python3
"""After a coordinator-released window, qualify the same Wasm in four engines."""
import argparse
import json
import os
from pathlib import Path
import sys

sys.dont_write_bytecode = True
from harness import Session, GuardFailure, digest, durable_json, require, unchanged_admission
from run_matrix import argv_for

SOURCE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--arena', type=Path, required=True)
    parser.add_argument('--runtime-sdk', type=Path, required=True)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cpu', type=int, default=8)
    args = parser.parse_args()
    build = json.loads((args.build / 'receipt.json').read_text())
    require(build['passed'] and build['native_oracle_passed'], 'actual native/link build qualification required')
    require(all(digest(path) == value for path, value in build['source_sha256'].items()), 'project source changed after build')
    wasm = Path(build['wasm'])
    require(digest(wasm) == build['wasm_sha256'], 'producer Wasm changed')
    require(args.out.is_absolute(), 'absolute new output directory required')
    args.out.mkdir(parents=True, exist_ok=False)
    data = {'passed': False, 'scope': 'ordinary generic managed-to-native C ABI in one WASIp1 memory',
            'ros_qualification': False, 'source_debugger_claim': False, 'debugger_executed': False,
            'build_receipt_sha256': digest(args.build / 'receipt.json'), 'wasm_sha256': digest(wasm),
            'runs': [], 'commands': [], 'helper_sha256': digest(__file__)}
    session = None
    try:
        session = Session(args.out, args.cpu)
        data.update(admission_before=session.before, boot=session.boot, cpu=args.cpu, commands=session.commands)
        env = dict(os.environ, LC_ALL='C', LANG='C')
        libraries = [args.runtime_sdk / 'lib/x86_64-unknown-linux-gnu', args.runtime_sdk / 'lib']
        env['LD_LIBRARY_PATH'] = ':'.join(str(path) for path in libraries) + (':' + env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
        expected = (SOURCE / 'expected-wasi.stdout').read_bytes()
        version = session.run([args.wasmtime, '--version'], 'wasmtime-version', env=env, timeout=30)
        data['wasmtime_version'] = Path(version['stdout']).read_text().strip()
        require(data['wasmtime_version'].startswith('wasmtime 42.0.1'), 'reuse the pinned Wasmtime42.0.1 reference')
        for engine in ('uwvm2', 'uwvm2-stable', 'uwvm2-ros', 'wasmtime'):
            row = {'engine': engine, 'passed': False}
            data['runs'].append(row)
            try:
                if engine == 'wasmtime':
                    binary = args.wasmtime
                    argv = [str(binary), 'run', '-C', 'cache=n', str(wasm)]
                else:
                    binary = args.arena / 'products' / engine / 'uwvm'
                    identity = json.loads((binary.parent / 'identity.json').read_text())
                    require(digest(binary) == identity['binary_sha256'], 'wrong UWVM product identity')
                    row['product'] = identity
                    argv = argv_for(binary, engine, {'wasm': str(wasm)})
                row['binary_sha256'] = digest(binary)
                execution = session.run(argv, engine, env=env, timeout=900)
                row['execution'] = execution
                require(Path(execution['stdout']).read_bytes() == expected, 'exact Wasm interop output mismatch')
                require(digest(binary) == row['binary_sha256'] and digest(wasm) == data['wasm_sha256'], 'input changed')
                row['passed'] = True
            except Exception as error:
                row['error'] = f'{type(error).__name__}: {error}'
                if isinstance(error, GuardFailure):
                    raise
                if session.commands and not session.commands[-1].get('retired', False):
                    raise RuntimeError('stop: owned process retirement not proved') from error
            durable_json(args.out / 'receipt.json', data)
        require(digest(args.build / 'receipt.json') == data['build_receipt_sha256'], 'build receipt changed')
        require(all(digest(path) == value for path, value in build['source_sha256'].items()), 'project source changed during run')
        require(digest(__file__) == data['helper_sha256'], 'runner changed')
        data['admission_after'] = unchanged_admission(session.before)
        data['passed'] = len(data['runs']) == 4 and all(row['passed'] for row in data['runs'])
    except BaseException as error:
        data['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        if session is not None:
            session.close()
        durable_json(args.out / 'receipt.json', data)
    raise SystemExit(0 if data['passed'] else 1)


if __name__ == '__main__':
    main()

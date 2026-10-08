#!/usr/bin/env python3
"""Run the real LLVM-full native-step fixture in the SSH Linux test cgroup."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


def sha(path):
    with path.open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--runtime-build', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    source, build, out = (path.resolve() for path in
                          (args.source_root, args.runtime_build, args.out))
    subprocess.run(['bash', str(source / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out.mkdir(parents=True, exist_ok=False)
    manifest = json.loads((build / 'build.json').read_text())
    runtime = build / 'runtime.o'
    assert runtime.is_file(), 'matching LLVM-full runtime object is required'
    before = sha(runtime)
    current_id = subprocess.check_output(['python3', str(source / 'tools/ci/wasm3_source_fingerprint.py'),
        str(source), str(out / 'source-before.json')], text=True).strip()
    assert current_id == manifest['source_id'], 'runtime source identity mismatch'
    command = list(manifest['commands']['cli'])
    main_source = 'src/uwvm2/uwvm/main.default.cpp'
    assert command.count(main_source) == 1
    fixture = source / 'test/0017.runtime/native_step_llvm_full.cc'
    assert fixture.is_file()
    command[command.index(main_source)] = str(fixture)
    assert command.count(str(runtime)) == 1
    command[command.index('-o') + 1] = str(out / 'probe')
    (out / 'link-command.json').write_text(json.dumps(command, indent=2) + '\n')
    with (out / 'link.log').open('wb') as log:
        result = subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, timeout=300)
    if result.returncode:
        raise RuntimeError((out / 'link.log').read_text(errors='replace')[-16000:])
    wat = '(module\n  (func (result i32)\n    i32.const 40\n    i32.const 2\n    i32.add))\n'
    (out / 'fixture.wat').write_text(wat)
    subprocess.run([str(args.wasm_tools), 'parse', str(out / 'fixture.wat'), '-o', str(out / 'fixture.wasm')], check=True)
    subprocess.run([str(args.wasm_tools), 'validate', str(out / 'fixture.wasm')], check=True)
    rows = []
    for policy in ('instruction', 'unwind'):
        run = [str(out / 'probe'), str(out / 'fixture.wasm'), policy]
        with (out / f'{policy}.log').open('wb') as log:
            result = subprocess.run(run, stdout=log, stderr=subprocess.STDOUT, timeout=60)
        output = (out / f'{policy}.log').read_text(errors='replace')
        rows.append({'command': run, 'exit': result.returncode,
                     'passed': result.returncode == 0 and 'PASS actual LLVM full native step' in output})
        (out / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        print(policy, output, flush=True)
        assert rows[-1]['passed'], f'{policy} product execution failed'
    assert sha(runtime) == before
    final_id = subprocess.check_output(['python3', str(source / 'tools/ci/wasm3_source_fingerprint.py'),
        str(source), str(out / 'source-after.json')], text=True).strip()
    assert final_id == current_id, 'source changed during product test'
    (out / 'summary.json').write_text(json.dumps({
        'passed': True, 'executions': len(rows), 'source_id': current_id,
        'runtime_sha256': before, 'probe_sha256': sha(out / 'probe'),
        'fixture_wasm_sha256': sha(out / 'fixture.wasm'),
        'scope': 'Actual LLVM full JIT PC ownership and exact zero/one-native-instruction SIGTRAP parks',
        'cgroup': Path('/proc/self/cgroup').read_text()}, indent=2) + '\n')
    subprocess.run(['bash', str(source / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)


if __name__ == '__main__':
    main()

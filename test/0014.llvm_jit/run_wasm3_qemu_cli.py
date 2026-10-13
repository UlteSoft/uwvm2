#!/usr/bin/env python3
"""Run new Core 3 syntax in a complete cross-built VM, including its live JIT.

The QEMU process executes the target LLVM library and generated target code.
This is separate from offline LLVM object emission and interpreter-only fixtures.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from run_wasm3_multi_memory import configurations

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('output', type=Path)
p.add_argument('--uwvm', type=Path, required=True)
p.add_argument('--qemu', type=Path, required=True)
p.add_argument('--sysroot', type=Path, required=True)
p.add_argument('--library-path', required=True)
p.add_argument('--wat2wasm', type=Path, required=True)
p.add_argument('--ros', action='store_true')
p.add_argument('--configuration', action='append', help='Select a runtime configuration; repeat to select several')
p.add_argument('--cpu', help='Explicit QEMU CPU model')
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
a.output.mkdir(parents=True, exist_ok=False)
for path in (a.uwvm, a.qemu, a.wat2wasm):
    if not path.is_file():
        p.error(f'missing executable: {path}')
wrapper = a.output / 'uwvm-qemu'
prefix = [str(a.qemu.resolve()), '-U', 'LD_LIBRARY_PATH', '-E', 'LD_LIBRARY_PATH=' + a.library_path,
          '-L', str(a.sysroot.resolve()), *(['-cpu', a.cpu] if a.cpu else []), str(a.uwvm.resolve())]
wrapper.write_text('#!/usr/bin/env python3\nimport os,sys\nargs=' + repr(prefix) + '\nos.execv(args[0],args+sys.argv[1:])\n')
wrapper.chmod(0o700)
metadata = dict(binary=str(a.uwvm.resolve()), binary_sha256=hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),
                qemu_command=prefix, product='uwvm2-ros' if a.ros else 'uwvm2',
                scope='actual target VM, live target LLVM JIT, new multi-memory syntax and nested trap stacks')
(a.output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
fixtures = a.output / 'fixtures'
subprocess.run([sys.executable, str(root / 'test/0013.uwvm_int/wasm3/multi_memory_cases.py'), str(fixtures)], check=True)
common = ['--uwvm', str(wrapper)] + (['--ros'] if a.ros else [])
rows = []
configs = [name for name, _ in configurations(wrapper, a.ros)]
configs.append('validator')
if a.configuration:
    unknown = set(a.configuration) - set(configs)
    if unknown:
        p.error('unknown configurations: ' + ', '.join(sorted(unknown)))
    configs = [name for name in configs if name in a.configuration]
for name in configs:
    suites = [('multi-memory', [sys.executable, str(root / 'test/0014.llvm_jit/run_wasm3_multi_memory.py'),
                                str(fixtures), str(a.output / 'multi-memory' / name), *common, '--configuration', name])]
    if name != 'validator':
        suites.append(('traps', [sys.executable, str(root / 'test/0014.llvm_jit/run_wasm3_multi_memory_traps.py'),
                                str(a.output / 'traps' / name), '--wat2wasm', str(a.wat2wasm),
                                *common, '--configuration', name]))
    for suite, command in suites:
        log_path = a.output / (name + '-' + suite + '.log')
        # One failed policy must not hide coverage of the remaining runtime modes.
        with log_path.open('wb') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        rows.append(dict(configuration=name, suite=suite, exit=result.returncode,
                         passed=result.returncode == 0, command=command, log=str(log_path)))
        (a.output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
        print(name, suite, 'PASS' if result.returncode == 0 else 'FAIL; see ' + str(log_path), flush=True)
raise SystemExit(any(not row['passed'] for row in rows))

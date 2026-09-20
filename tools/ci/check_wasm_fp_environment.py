#!/usr/bin/env python3
"""Build/run the FP boundary tests with a native compiler or an explicitly supplied cross compiler/runner."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cxx', default='c++')
parser.add_argument('--cxxflag', action='append', default=[])
parser.add_argument('--runner', default='', help='e.g. "qemu-ppc64 -L /path/to/powerpc64-linux-gnu"')
parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
parser.add_argument('--build-dir', type=Path)
args = parser.parse_args()
output = args.build_dir or Path(tempfile.mkdtemp(prefix='uwvm-fp-'))
output.mkdir(parents=True, exist_ok=True)
results = []
for name in ('llvm_wasm_fp_environment', 'wasm_fp_control', 'wasm_fp_arch_environment',
             'wasm_fp_fixed_contract', 'wasm_fp_ppc_generic', 'strict_float'):
    binary = output.resolve() / name
    command = [args.cxx, '-std=c++20', '-O3', '-Wall', '-Wextra', *args.cxxflag,
               '-I', str(args.source_root / 'src'), str(args.source_root / 'test/0017.runtime' / (name + '.cc')),
               '-o', str(binary), '-lm']
    build = subprocess.run(command, capture_output=True, text=True)
    (output / (name + '.build.log')).write_text(build.stdout + build.stderr)
    run_command = [*shlex.split(args.runner), str(binary)]
    result = subprocess.run(run_command, capture_output=True, text=True, timeout=60) if build.returncode == 0 else None
    record = {'test': name, 'build_command': command, 'run_command': run_command, 'build': build.returncode,
              'run': result.returncode if result else None, 'output': result.stdout + result.stderr if result else ''}
    results.append(record)
    print(name, 'PASS' if record['build'] == 0 and record['run'] == 0 else 'FAIL', flush=True)
(output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
print('Evidence:', output)
raise SystemExit(any(r['build'] != 0 or r['run'] != 0 for r in results))

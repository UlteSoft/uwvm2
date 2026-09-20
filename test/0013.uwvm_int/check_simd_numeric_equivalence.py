#!/usr/bin/env python3
"""Compare every SIMD unary/binary evaluator across explicit compiler profiles.

Use the cxx/run/flags/ldflags/env/optimizations config format of
run_fp_cross_matrix.py. The first successful profile supplies the differential
reference; this is not an independent specification oracle. Fixed-bit tests
must also pass. Each executable emits 170 opcodes x 832 mixed-lane inputs.
Apply aggregate memory/CPU limits outside this serial driver.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--config', type=Path, required=True)
p.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2])
p.add_argument('--out', type=Path, required=True)
p.add_argument('--timeout', type=int, default=600)
a = p.parse_args()
if a.timeout <= 0:
    p.error('timeout must be positive')
repo = a.source_root.resolve()
a.out.mkdir(parents=True, exist_ok=False)
out = a.out.resolve()
profiles = json.loads(a.config.read_text())
names = [profile['name'] for profile in profiles]
if not names or len(names) != len(set(names)) or any(not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', n) for n in names):
    p.error('profile names must be nonempty, unique and safe path components')
source = repo/'test/0013.uwvm_int/fixtures/simd_numeric_dump.cpp'
inputs = [source, repo/'src/uwvm2/runtime/compiler/shared/wasm1p1_simd.h',
          repo/'src/uwvm2/runtime/compiler/shared/strict_float.h',
          repo/'src/uwvm2/parser/wasm/standard/wasm1p1/type/value_type.h']
provenance = {str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}
rows, reference = [], None

def invoke(command, stdout, stderr, environment):
    with stdout.open('wb') as output, stderr.open('wb') as log:
        child = subprocess.Popen(command, stdout=output, stderr=log, env=environment,
                                 start_new_session=os.name == 'posix')
        try:
            return child.wait(timeout=a.timeout)
        except subprocess.TimeoutExpired:
            if os.name == 'posix':
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            else:
                child.kill()
            child.wait()
            return 124

for profile in profiles:
    for optimization in profile.get('optimizations', ['O0', 'O3']):
        if optimization not in ('O0', 'O1', 'O2', 'O3', 'Os', 'Oz'):
            p.error('invalid optimization')
        label = profile['name']+'-'+optimization
        binary, dump = out/label, out/(label+'.txt')
        command = [*profile['cxx'], '-std=c++26', '-'+optimization, '-ffp-contract=off',
                   '-DUWVM_DISABLE_JIT', '-DFAST_IO_DISABLE_FLOATING_POINT', *profile.get('flags', [])]
        for include in ('src', 'third-parties/fast_io/include', 'third-parties/bizwen/include', 'third-parties/boost_unordered/include'):
            command += ['-I'+str(repo/include)]
        command += [str(source), '-o', str(binary), '-lm', *profile.get('ldflags', [])]
        run = [*profile['run'], str(binary)]
        row = dict(label=label, build_command=command, run_command=run, source_sha256=provenance, passed=False)
        rows.append(row)
        environment = {**os.environ, **profile.get('env', {})}
        try:
            row['build'] = invoke(command, out/(label+'.build.out'), out/(label+'.build.log'), environment)
            if row['build'] == 0:
                row['run'] = invoke(run, dump, out/(label+'.run.log'), environment)
                if row['run'] == 0:
                    data = dump.read_bytes()
                    row['records'] = data.count(b'\n')
                    row['output_sha256'] = hashlib.sha256(data).hexdigest()
                    if row['records'] == 170 * 832:
                        if reference is None:
                            reference = (label, data)
                        row['reference'] = reference[0]
                        row['passed'] = data == reference[1]
        except OSError as error:
            row['error'] = str(error)
        (out/'results.json').write_text(json.dumps(rows, indent=2)+'\n')
        print(label, 'PASS' if row['passed'] else 'FAIL', flush=True)
if len(rows) < 2:
    p.error('a differential test requires at least two profiles/optimization runs')
raise SystemExit(any(not row['passed'] for row in rows))

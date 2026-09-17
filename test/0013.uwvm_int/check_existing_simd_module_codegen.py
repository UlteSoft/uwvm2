#!/usr/bin/env python3
"""Compare an existing xmake Linux/x86_64 release SIMD BMI with header codegen.

Only reads the source/build trees; generated objects and logs go to a new --out
folder. Use the exact compiler used to create the BMI. This checks eight native
shared-evaluator wrappers, not a linked VM runtime or a Wasm JIT product.
--execute-fixture additionally runs the rounding fixture's independent bit
oracle in both forms; it still does not execute a complete VM module build.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source-root', type=Path, required=True)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--fixture', type=Path, required=True)
p.add_argument('--cxx', required=True)
p.add_argument('--objdump', required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--execute-fixture', action='store_true', help='link/run uwvm_int_simd_rounding.cc in both forms')
a = p.parse_args()
if platform.system() != 'Linux' or platform.machine().lower() not in ('x86_64', 'amd64'):
    print('SKIP: this xmake-cache regression requires native Linux x86_64')
    raise SystemExit(77)
a.out.mkdir(parents=True, exist_ok=False)
dep = a.build / '.deps/uwvm/linux/x86_64/release/src/uwvm2/runtime/compiler/shared/wasm1p1_simd.cppm.d'
values = dep.read_text().split('    values = {', 1)[1].split('\n    }', 1)[0]
flags = [json.loads(x) for x in re.findall(r'"(?:[^"\\]|\\.)*"', values)]
bmis = sorted((a.build / '.gens/uwvm/linux/x86_64/release/rules/bmi/cache/interfaces').rglob('*.pcm'))
mapping = {bmi.stem.replace('_PARTITION_', ':'): str(bmi) for bmi in bmis}
if len(mapping) != len(bmis):
    raise RuntimeError('ambiguous BMI inventory')
target = 'uwvm2.runtime.compiler.shared.wasm1p1_simd'
target_bmi = Path(mapping[target])
before = hashlib.sha256(target_bmi.read_bytes()).hexdigest()
commands = []

def run(command, label):
    command = [str(x) for x in command]
    commands.append(command)
    (a.out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    result = subprocess.run(command, cwd=a.source_root, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=240)
    (a.out / (label + '.log')).write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(f'{label} failed with {result.returncode}')
    return result.stdout

profiles = {}
executions = {}
for profile in ('header', 'module'):
    mode_flags = [x for x in flags if profile == 'module' or x != '-DUWVM_MODULE']
    if profile == 'module':
        mode_flags += ['-DUWVM2TEST_SIMD_NAMED_MODULE',
                       *[f'-fmodule-file={name}={bmi}' for name, bmi in mapping.items()]]
    obj = a.out / (profile + '.o')
    run([a.cxx, *mode_flags, '-c', a.fixture, '-o', obj], profile + '-compile')
    asm = run([a.objdump, '-d', '--no-show-raw-insn', obj], profile + '-asm')
    functions = dict(re.findall(r'^[0-9a-f]+ <([^>]+)>:\n(.*?)(?=^[0-9a-f]+ <|\Z)', asm, re.M | re.S))
    checks = {}
    for width in (32, 64):
        for op in ('ceil', 'floor', 'trunc', 'nearest'):
            name = f'round_f{width}_{op}'
            body = functions[name]
            instructions = re.findall(r'^\s*[0-9a-f]+:\s+([a-z][a-z0-9.]*)\b([^\n]*)', body, re.M)
            useful = []
            for ins, operands in instructions:
                if ins.startswith('nop') or ins == 'int3':
                    continue
                useful.append([ins, operands.strip()])
                if ins.startswith('ret'):
                    break
            if not useful or any(ins.startswith('call') for ins, _ in useful):
                raise RuntimeError(f'{profile}/{name}: native helper call or empty code')
            if re.search(r'%[er](?:sp|bp)\b', '\n'.join(x[1] for x in useful)):
                raise RuntimeError(f'{profile}/{name}: unexpected stack access')
            packed = 'ps' if width == 32 else 'pd'
            if sum(bool(re.fullmatch(r'(?:v?round|vrndscale)' + packed, ins)) for ins, _ in useful) != 1:
                raise RuntimeError(f'{profile}/{name}: expected one packed rounding instruction')
            checks[name] = useful
    profiles[profile] = checks
    if a.execute_fixture:
        # These evaluator templates have no dependency on the VM's runtime
        # library. Do not substitute header output for a module execution: link
        # and run each separately compiled object and require the bit oracle.
        binary = a.out / (profile + '-fixture')
        run([a.cxx, *mode_flags, obj, '-o', binary], profile + '-link')
        output = run([binary.resolve()], profile + '-run')
        if 'SIMD rounding/transport checks=792 failures=0' not in output:
            raise RuntimeError(f'{profile}: incomplete rounding/transport oracle')
        executions[profile] = dict(checks=792, failures=0)
if profiles['header'] != profiles['module']:
    raise RuntimeError('header/module rounding instruction sequences differ')
after = hashlib.sha256(target_bmi.read_bytes()).hexdigest()
if before != after:
    raise RuntimeError('BMI changed during comparison')
result = dict(functions_checked=8, identical_header_module_code=True,
              module_bmi_sha256=before, fixture_sha256=hashlib.sha256(a.fixture.read_bytes()).hexdigest(),
              runtime_module_execution=False, evaluator_executions=executions, profiles=profiles)
(a.out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result), flush=True)

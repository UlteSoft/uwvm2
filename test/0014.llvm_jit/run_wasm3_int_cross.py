#!/usr/bin/env python3
"""Run the real Wasm 3 parser, initializer, validators and interpreter under QEMU.

This cross-compiles the new-syntax fixture driver, including both uncached and
musttail register-ring execution. It is not a cross-target LLVM JIT runtime test.
Clang compiles C++; the matching complete LLVM build supplies object emission.
No source/IR rewriting removes musttail or changes the production interpreter.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import gzip
import shutil
from pathlib import Path
import resource
import subprocess
from run_wasm3_relaxed_cross import PROFILES as BASE_PROFILES

# POWER10 ELFv2 PC-relative calls are a distinct ABI capability. Keep the older
# PowerPC failures visible; this profile never relabels a POWER8 result.
PROFILES = [*BASE_PROFILES, ('ppc64le-pcrel', 'powerpc64le-linux-gnu', 'pwr10',
             '+vsx,+pcrelative-memops', 'ppc64le', 'native', ['-mcpu=power10', '-mpcrel'])]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('output', type=Path)
    p.add_argument('--repository', type=Path, required=True)
    p.add_argument('--clang', type=Path, required=True)
    p.add_argument('--llvm', type=Path, required=True)
    p.add_argument('--deps', type=Path, required=True)
    p.add_argument('--only', help='comma separated profile names')
    p.add_argument('--jobs', type=int, default=1)
    p.add_argument('--suite', choices=('multi-memory', 'store-atomicity', 'wait-notify', 'tail-call', 'ref-branches'), default='multi-memory')
    p.add_argument('--dispatch', choices=('both', 'uncached'), default='both',
                   help='uncached is a separate run, never a relabeling of failed musttail coverage')
    p.add_argument('--store-growth', action='store_true', help='also check stores before/after memory.grow')
    p.add_argument('--wat2wasm', type=Path, help='required for the store-atomicity fixtures')
    a = p.parse_args()
    if a.suite == 'store-atomicity' and a.wat2wasm is None:
        p.error('store-atomicity requires --wat2wasm')
    if a.store_growth and a.suite != 'store-atomicity':
        p.error('--store-growth requires --suite store-atomicity')
    if not 1 <= a.jobs <= 2:
        p.error('jobs must be 1 or 2; C++ template compilation can exceed 20 GiB per process')
    a.output.mkdir(parents=True, exist_ok=True)
    selected = set(a.only.split(',')) if a.only else {x[0] for x in PROFILES}
    if not selected <= {x[0] for x in PROFILES}:
        p.error('unknown profile')
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    subprocess.run(['bash', str(a.repository / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    fingerprint = ['python3', str(a.repository / 'tools/ci/wasm3_source_fingerprint.py'), str(a.repository)]
    before = subprocess.check_output(fingerprint + [str(a.output / 'source-manifest.json')], text=True).strip()
    if a.suite == 'multi-memory':
        subprocess.run(['python3', str(a.repository / 'test/0013.uwvm_int/wasm3/multi_memory_cases.py'), str(a.output)], check=True)
        executions = [('run', [])]
    elif a.suite in ('wait-notify', 'tail-call', 'ref-branches'):
        executions = [('run', [])]
    else:
        subprocess.run(['python3', str(a.repository / 'test/0013.uwvm_int/wasm3/memory_store_atomicity_cases.py'),
                        str(a.output / 'fixtures'), '--wat2wasm', str(a.wat2wasm)], check=True)
        executions = [('run-' + case['opcode'], [case['wasm'], str(case['width']), *(['growth'] if a.store_growth else [])])
                      for case in json.loads((a.output / 'fixtures/cases.json').read_text())]

    def run_profile(profile):
        name, triple, cpu, features, emulator, _, flags = profile
        directory = a.output / name
        directory.mkdir(exist_ok=False)
        prefix = directory / a.suite
        path = lambda suffix: str(prefix) + suffix
        gcc = a.deps / 'usr/lib/gcc-cross' / triple / '15'
        sysroot = a.deps / 'usr' / triple
        compiler = [str(a.clang), f'--target={triple}', f'--sysroot={a.deps}', f'--gcc-install-dir={gcc}',
                    '-idirafter', str(sysroot / 'include'), '-stdlib=libstdc++', '-fuse-ld=lld', '-O2', *flags]
        includes = sum((['-I', str(a.repository / entry)] for entry in
                        ('src', 'third-parties/bizwen/include', 'third-parties/fast_io/include', 'third-parties/boost_unordered/include')), [])
        target = ['-Xclang', '-target-cpu', '-Xclang', cpu]
        for feature in features.split(','):
            target += ['-Xclang', '-target-feature', '-Xclang', feature]
        source = a.repository / 'test/0013.uwvm_int/wasm3' / (
            'ref_branches.cc' if a.suite == 'ref-branches' else 'tail_call.cc' if a.suite == 'tail-call' else 'multi_memory.cc' if a.suite == 'multi-memory' else 'threads_wait_notify.cc' if a.suite == 'wait-notify' else 'memory_store_atomicity.cc')
        definitions = ['-DUWVM=2', '-DUWVM_USE_UWVM_INT', '-DUWVM_DISABLE_JIT', '-DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1']
        if a.dispatch == 'uncached':
            definitions.append('-DUWVM2TEST_TAIL_BYREF_ONLY' if a.suite == 'tail-call' else '-DUWVM2TEST_UNCACHED_ONLY')
        linker = a.deps / 'usr/bin' / (triple + '-ld')
        link_flags = ['--ld-path=' + str(linker)] if triple in {
            'powerpc64-linux-gnu', 'powerpc-linux-gnu', 'sparc64-linux-gnu'} else []
        emulator_options = ['-cpu', 'power10'] if name == 'ppc64le-pcrel' else []
        codegen = [str(a.llvm / 'llc'), '-O2', '-verify-machineinstrs', '-relocation-model=pic', f'-mcpu={cpu}', f'-mattr={features}']
        if triple.startswith('mips'):
            # Match the production Clang MIPS build contract; musttail stays mandatory.
            codegen.append('-mips-tail-calls')
        phases = [
            ('clang', [*compiler, *target, '-std=c++26', '-g0', '-ferror-limit=1', '-Wno-undefined-inline', *includes, '-I', str(a.output), *definitions,
                       '-S', '-emit-llvm', str(source), '-o', path('.ll')]),
            ('object', [*codegen, '-filetype=obj', path('.ll'), '-o', path('.o')]),
            ('assembly', [*codegen, '-filetype=asm', path('.ll'), '-o', path('.s')]),
            ('link', [*compiler, *link_flags, path('.o'), '-L' + str(sysroot / 'lib'), '-latomic', '-pthread', '-o', path('.test')]),
        ]
        phases += [(label, [str(a.deps / 'usr/bin' / ('qemu-' + emulator)), *emulator_options,
                           '-U', 'LD_LIBRARY_PATH', '-L', str(sysroot), path('.test'), *arguments])
                   for label, arguments in executions]
        row = dict(profile=name, triple=triple, cpu=cpu, features=features, suite=a.suite, dispatch=a.dispatch, store_growth=a.store_growth, phases=[])
        for phase, command in phases:
            try:
                run = subprocess.run(command, capture_output=True, timeout=900 if phase == 'clang' else 300)
                status, output = run.returncode, run.stdout + run.stderr
            except subprocess.TimeoutExpired as e:
                status, output = 'timeout', (e.stdout or b'') + (e.stderr or b'')
            (directory / (phase + '.log')).write_bytes(output)
            row['phases'].append(dict(phase=phase, command=command, exit=status))
            if status != 0:
                break
        row['passed'] = phase == executions[-1][0] and status == 0
        # Preserve the complete reviewable IR/assembly without filling the remote disk.
        row['compressed_artifacts'] = []
        for suffix in ('.ll', '.s'):
            file = Path(path(suffix))
            if file.is_file():
                with file.open('rb') as inp, gzip.open(str(file) + '.gz', 'wb', compresslevel=3) as out:
                    shutil.copyfileobj(inp, out)
                file.unlink()
                row['compressed_artifacts'].append(str(file) + '.gz')
        (directory / 'result.json').write_text(json.dumps(row, indent=2) + '\n')
        diagnostics = output.decode(errors='replace').splitlines()
        primary = next((line for line in diagnostics if ': error:' in line or 'LLVM ERROR:' in line or 'Bad machine code:' in line), None)
        row['failure_summary'] = None if row['passed'] else (primary or '\n'.join(diagnostics[:8]))[:1500]
        (directory / 'result.json').write_text(json.dumps(row, indent=2) + '\n')
        print(name, f'PASS {a.suite}: actual parser/validator/initializer, dispatch={a.dispatch}' if row['passed'] else
              f'FAIL {phase}: {row["failure_summary"]}', flush=True)
        return row

    rows = []
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        jobs = [pool.submit(run_profile, x) for x in PROFILES if x[0] in selected]
        for job in as_completed(jobs):
            rows.append(job.result())
            (a.output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
    after = subprocess.check_output(fingerprint + [str(a.output / 'source-manifest-after.json')], text=True).strip()
    if before != after:
        raise RuntimeError('Production sources changed during cross compilation; results are invalid')
    return not all(row['passed'] for row in rows)


if __name__ == '__main__':
    raise SystemExit(main())

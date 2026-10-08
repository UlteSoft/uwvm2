#!/usr/bin/env python3
"""Qualify fault-first Core 3 stores in the frozen full interpreter and JIT.

Run inside the established 64 GiB / swap0 / 20-CPU Docker cgroup. The input
fixtures are separately validated by wasm-tools and actually run by Wasmtime.
This runner observes every committed byte from the trap callback; an ordinary
CLI trap message cannot prove that a split store did not write a prefix.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import time


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: object) -> None:
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def cgroup() -> dict:
    base = Path('/sys/fs/cgroup')
    result = {key: (base / key).read_text().strip()
              for key in ('memory.max', 'memory.swap.max', 'cpuset.cpus.effective')}
    if result != {'memory.max': '68719476736', 'memory.swap.max': '0',
                  'cpuset.cpus.effective': '0,2,4,6,16-31'}:
        raise RuntimeError(f'wrong test/build cgroup: {result}')
    result['memory.events'] = (base / 'memory.events').read_text().strip()
    return result


def invoke(command: list[str], *, cwd: Path, log: Path, timeout: int) -> None:
    with log.open('w') as output:
        completed = subprocess.run(command, cwd=cwd, stdout=output,
                                   stderr=subprocess.STDOUT, timeout=timeout,
                                   env=os.environ.copy())
    if completed.returncode:
        raise RuntimeError(f'exit {completed.returncode}: {command}; see {log}')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--normal-build-json', type=Path, required=True)
    parser.add_argument('--fixtures32', type=Path, required=True)
    parser.add_argument('--fixtures64', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', default='/toolchain/bin/clang++')
    parser.add_argument('--preflight-only', action='store_true',
                        help='verify source, O3 object, Wasm oracles, and cgroup without compiling')
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    before = cgroup()
    manifest = output / 'source-manifest-before.json'
    fingerprint = source / 'tools/ci/wasm3_source_fingerprint.py'
    source_id = subprocess.check_output(['python3', str(fingerprint),
                                          str(source), str(manifest)], text=True).strip()
    build = json.loads(args.normal_build_json.read_text())
    if source_id != build['source_id'] or Path(build['source']).resolve() != source:
        raise RuntimeError('source snapshot does not match the prebuilt full-JIT runtime')
    runtime = Path(build['runtime_command'][-1])
    if digest(runtime) != build['runtime_object_sha256']:
        raise RuntimeError('prebuilt runtime object differs from its O3 source-bound build')
    cli = Path(build['cli_command'][-1])
    if digest(cli) != build['binary_sha256']:
        raise RuntimeError('prebuilt CLI differs from its O3 source-bound build')
    cases = []
    references = {}
    for fixture_dir, memory64 in ((args.fixtures32.resolve(), False),
                                  (args.fixtures64.resolve(), True)):
        manifest_path = fixture_dir / 'cases.json'
        reference_path = fixture_dir / 'reference.json'
        rows = json.loads(manifest_path.read_text())
        reference = json.loads(reference_path.read_text())
        # The original memory32 manifest predates its explicit memory64 field.
        if len(rows) != 26 or any(row.get('memory64', False) != memory64 for row in rows):
            raise RuntimeError(f'incomplete memory{"64" if memory64 else "32"} cases: {fixture_dir}')
        if (reference['cases'] != len(rows) or reference['memory64'] != memory64
                or not reference['all_wasm_tools_validate'] or not reference['all_wasmtime_run_32']
                or reference['case_manifest_sha256'] != digest(manifest_path)):
            raise RuntimeError(f'incomplete Wasm oracle provenance: {reference_path}')
        references[str(fixture_dir)] = {'manifest_sha256': digest(manifest_path),
                                        'reference_sha256': digest(reference_path),
                                        **reference}
        for row in rows:
            wasm = Path(row['wasm'])
            if wasm.resolve().parent != fixture_dir or not wasm.is_file():
                raise RuntimeError(f'foreign fixture: {wasm}')
            cases.append((row, wasm, digest(wasm)))
    if args.preflight_only:
        preflight = {'source_id': source_id, 'source_manifest_sha256': digest(manifest),
                     'runtime_object_sha256': digest(runtime), 'cases': len(cases),
                     'normal_cli_sha256': digest(cli),
                     'wasm_oracles': references, 'cgroup': before}
        write_json(output / 'preflight.json', preflight)
        print(f'PREFLIGHT PASS {source_id}, {len(cases)} Wasm fixtures')
        return

    int_flags = ['-std=c++26', '-stdlib=libc++', '-fuse-ld=lld',
                 '-rtlib=compiler-rt', '-unwindlib=libunwind', '-O3', '-g0',
                 '-Wno-undefined-inline', '-DUWVM=2', '-DUWVM_USE_UWVM_INT',
                 '-DUWVM_DISABLE_JIT', '-DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1',
                 f'-DUWVM2_BUILD_SOURCE_ID=u8"{source_id}"',
                 '-I', 'src', '-I', 'third-parties/bizwen/include',
                 '-I', 'third-parties/fast_io/include',
                 '-I', 'third-parties/boost_unordered/include']
    full_tuning = ['-DUWVM_ENABLE_UWVM_INT_COMBINE_OPS',
                   '-DUWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS',
                   '-DUWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS',
                   '-DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT',
                   '-DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY']
    int_source = 'test/0013.uwvm_int/wasm3/memory_store_atomicity.cc'
    jit_source = 'test/0013.uwvm_int/wasm3/memory_store_atomicity_jit.cc'
    binaries = {}
    for name, tuning in (('int-normal', []), ('int-all-combine-delay', full_tuning)):
        target = output / name
        command = [args.compiler, *int_flags, *tuning, int_source, '-o', str(target)]
        write_json(output / f'{name}.command.json', command)
        invoke(command, cwd=source, log=output / f'{name}.build.log', timeout=1800)
        binaries[name] = {'path': str(target), 'sha256': digest(target), 'command': command}

    original = build['cli_command']
    if original.count('src/uwvm2/uwvm/main.default.cpp') != 1:
        raise RuntimeError('unexpected source-bound full-JIT link command')
    target = output / 'jit-full'
    command = [jit_source if token == 'src/uwvm2/uwvm/main.default.cpp' else token
               for token in original[:-2]] + ['-O1', '-o', str(target)]
    write_json(output / 'jit-full.command.json', command)
    invoke(command, cwd=source, log=output / 'jit-full.build.log', timeout=1800)
    binaries['jit-full'] = {'path': str(target), 'sha256': digest(target), 'command': command}

    results = []
    for backend in ('int-normal', 'int-all-combine-delay', 'jit-full'):
        policies = ('instruction', 'unwind') if backend == 'jit-full' else (None,)
        for policy in policies:
            for growth in (False, True):
                for row, wasm, wasm_hash in cases:
                    command = [binaries[backend]['path'], str(wasm), str(row['width'])]
                    if policy is not None:
                        command.append(policy)
                    if growth:
                        command.append('growth')
                    if row.get('memory64', False):
                        command.append('memory64')
                    started = time.monotonic()
                    completed = subprocess.run(command, cwd=source, capture_output=True,
                                               text=True, timeout=120)
                    item = {'backend': backend, 'policy': policy, 'growth': growth,
                            'memory64': row.get('memory64', False), 'opcode': row['opcode'],
                            'width': row['width'], 'fixture': str(wasm),
                            'fixture_sha256': wasm_hash, 'command': command,
                            'exit': completed.returncode,
                            'seconds': time.monotonic() - started,
                            'stdout': completed.stdout, 'stderr': completed.stderr,
                            'prefix_fault_checks': (2 if backend.startswith('int') else 1)
                                                   * (2 if growth else 1) * (row['width'] - 1),
                            'positive_old_boundary_writes': (2 if backend.startswith('int') else 1)
                                                            * (row['width'] - 1) if growth else 0}
                    results.append(item)
                    write_json(output / 'results.json', results)
                    if completed.returncode:
                        raise RuntimeError(f'split-store check failed: {item}')

    after_id = subprocess.check_output(['python3', str(fingerprint), str(source),
                                         str(output / 'source-manifest-after.json')], text=True).strip()
    after = cgroup()
    if after_id != source_id:
        raise RuntimeError('production source changed during qualification')
    for event in ('oom ', 'oom_kill '):
        before_value = next(int(x.split()[1]) for x in before['memory.events'].splitlines() if x.startswith(event))
        after_value = next(int(x.split()[1]) for x in after['memory.events'].splitlines() if x.startswith(event))
        if after_value != before_value:
            raise RuntimeError(f'cgroup {event.strip()} increased during qualification')
    expected_runs = len(cases) * 8
    expected_fault_checks = sum(row['width'] - 1 for row, _, _ in cases) * 18
    expected_old_boundary_writes = sum(row['width'] - 1 for row, _, _ in cases) * 6
    if (len(results) != expected_runs or
            sum(item['prefix_fault_checks'] for item in results) != expected_fault_checks or
            sum(item['positive_old_boundary_writes'] for item in results) != expected_old_boundary_writes):
        raise RuntimeError('incomplete int/JIT, instruction/unwind, memory32/64, growth coverage')
    summary = {'source_id': source_id, 'source_before_sha256': digest(manifest),
               'source_after_sha256': digest(output / 'source-manifest-after.json'),
               'normal_build_json': str(args.normal_build_json.resolve()),
               'normal_build_json_sha256': digest(args.normal_build_json),
               'runtime_object_sha256': digest(runtime),
               'normal_cli_sha256': digest(cli), 'binaries': binaries,
               'test_inputs_sha256': {
                   str(Path(__file__).resolve()): digest(Path(__file__).resolve()),
                   int_source: digest(source / int_source),
                   jit_source: digest(source / jit_source),
                   'test/0013.uwvm_int/wasm3/memory_store_atomicity_cases.py':
                       digest(source / 'test/0013.uwvm_int/wasm3/memory_store_atomicity_cases.py'),
                   'test/0013.uwvm_int/strict/uwvm_int_translate_strict_common.h':
                       digest(source / 'test/0013.uwvm_int/strict/uwvm_int_translate_strict_common.h'),
                   str(Path(args.compiler).resolve()): digest(Path(args.compiler).resolve())},
               'wasm_oracles': references,
               'cgroup_before': before, 'cgroup_after': after, 'cases': len(results),
               'prefix_fault_checks': sum(item['prefix_fault_checks'] for item in results),
               'positive_old_boundary_writes': sum(item['positive_old_boundary_writes'] for item in results),
               'memory_types': ['memory32', 'memory64'],
               'store_classes': ['scalar', 'SIMD', 'SIMD lane', 'local-value', 'same-memory-copy'],
               'scope': 'two-memory target-memory fault-first; int uncached/register-ring and JIT full instruction/unwind'}
    write_json(output / 'summary.json', summary)
    print(f"PASS {len(results)} runs, {summary['prefix_fault_checks']} fault-prefix checks, "
          f"{summary['positive_old_boundary_writes']} after-grow positive writes")


if __name__ == '__main__':
    main()

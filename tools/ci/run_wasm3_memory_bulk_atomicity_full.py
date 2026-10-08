#!/usr/bin/env python3
"""Source-bound, same-instance memory.copy/init fault-first qualification.

Build only inside the verified Linux cgroup. Each child traps inside the same
runtime module whose source and target memories its callback then reads byte by
byte. The Wasmtime oracle checks independent positive fixture execution only.
"""

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
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
    root = Path('/sys/fs/cgroup')
    limits = {key: (root / key).read_text().strip() for key in
              ('memory.max', 'memory.swap.max', 'cpuset.cpus.effective')}
    expected = {'memory.max': '68719476736', 'memory.swap.max': '0',
                'cpuset.cpus.effective': '0,2,4,6,16-31'}
    if limits != expected:
        raise RuntimeError(f'wrong test/build cgroup: {limits}')
    limits['memory.events'] = (root / 'memory.events').read_text().strip()
    return limits


def event(limits: dict, name: str) -> int:
    return next(int(line.split()[1]) for line in limits['memory.events'].splitlines()
                if line.startswith(name + ' '))


def invoke(command: list[str], *, cwd: Path, log: Path, timeout: int) -> None:
    with log.open('w') as stream:
        completed = subprocess.run(command, cwd=cwd, stdout=stream,
                                   stderr=subprocess.STDOUT, timeout=timeout,
                                   env=os.environ.copy())
    if completed.returncode:
        raise RuntimeError(f'exit {completed.returncode}: {command}; see {log}')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--normal-build-json', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', default='/toolchain/bin/clang++')
    parser.add_argument('--preflight-only', action='store_true')
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    before = cgroup()
    fingerprint = source / 'tools/ci/wasm3_source_fingerprint.py'
    before_manifest = output / 'source-manifest-before.json'
    source_id = subprocess.check_output(['python3', str(fingerprint), str(source),
                                         str(before_manifest)], text=True).strip()
    build = json.loads(args.normal_build_json.read_text())
    if source_id != build['source_id'] or Path(build['source']).resolve() != source:
        raise RuntimeError('normal O3 JIT runtime is not source-matched')
    runtime = Path(build['runtime_command'][-1])
    cli = Path(build['cli_command'][-1])
    if digest(runtime) != build['runtime_object_sha256'] or digest(cli) != build['binary_sha256']:
        raise RuntimeError('prebuilt O3 object or CLI SHA changed')
    reference_path = args.fixtures.resolve() / 'reference.json'
    reference = json.loads(reference_path.read_text())
    generator = source / 'test/0013.uwvm_int/wasm3/memory_bulk_atomicity_cases.py'
    if digest(generator) != reference.get('generator_sha256'):
        raise RuntimeError('fixture generator bytes differ from Wasm oracle provenance')
    cases = reference['cases']
    expected_names = {f'memory-{op}-m{bits}' for op in ('copy', 'init')
                      for bits in (32, 64)}
    if len(cases) != 4 or {case['name'] for case in cases} != expected_names:
        raise RuntimeError('incomplete copy/init memory32/64 fixture set')
    for case in cases:
        wasm = Path(case['wasm']).resolve()
        if wasm.parent != args.fixtures.resolve() or digest(wasm) != case['wasm_sha256'] or \
           digest(wasm.with_suffix('.wat')) != case['wat_sha256'] or \
           not case['wasm_tools_validated'] or not case['wasmtime_start_executed'] or \
           len(case.get('wasmtime_oob_oracle', [])) != 4:
            raise RuntimeError(f'fixture oracle or hash mismatch: {case}')
    if args.preflight_only:
        write_json(output / 'preflight.json', {'source_id': source_id,
                   'source_manifest_sha256': digest(before_manifest),
                   'normal_cli_sha256': digest(cli),
                   'normal_runtime_object_sha256': digest(runtime),
                   'fixture_reference_sha256': digest(reference_path),
                   'fixtures': cases, 'cgroup': before})
        print('PREFLIGHT PASS 4 Wasm oracle fixtures and source-bound O3 runtime')
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
    int_file = 'test/0013.uwvm_int/wasm3/memory_bulk_atomicity.cc'
    jit_file = 'test/0013.uwvm_int/wasm3/memory_bulk_atomicity_jit.cc'
    common_file = 'test/0013.uwvm_int/wasm3/memory_bulk_atomicity_common.h'
    binaries = {}
    for name, tuning in (('int-normal', []), ('int-all-combine-delay', full_tuning)):
        target = output / name
        command = [args.compiler, *int_flags, *tuning, int_file, '-o', str(target)]
        write_json(output / f'{name}.command.json', command)
        invoke(command, cwd=source, log=output / f'{name}.build.log', timeout=1800)
        binaries[name] = {'path': str(target), 'sha256': digest(target),
                          'command': command}

    original = build['cli_command']
    main_indices = [i for i, token in enumerate(original)
                    if token.endswith('src/uwvm2/uwvm/main.default.cpp')]
    if len(main_indices) != 1 or original[-2] != '-o':
        raise RuntimeError('unrecognized source-bound full-JIT link command')
    target = output / 'jit-full'
    command = [jit_file if i == main_indices[0] else token
               for i, token in enumerate(original[:-2])] + ['-O1', '-o', str(target)]
    write_json(output / 'jit-full.command.json', command)
    invoke(command, cwd=source, log=output / 'jit-full.build.log', timeout=1800)
    binaries['jit-full'] = {'path': str(target), 'sha256': digest(target),
                            'command': command}

    results = []
    for backend in ('int-normal', 'int-all-combine-delay', 'jit-full'):
        for policy in (('instruction', 'unwind') if backend == 'jit-full' else (None,)):
            for case in cases:
                command = [binaries[backend]['path'], case['wasm'], case['operation'],
                           'memory64' if case['memory64'] else 'memory32']
                if policy:
                    command.append(policy)
                started = time.monotonic()
                completed = subprocess.run(command, cwd=source, capture_output=True,
                                           text=True, timeout=120)
                dispatches = 2 if backend.startswith('int') else 1
                result = {'backend': backend, 'policy': policy, 'fixture': case['wasm'],
                          'fixture_sha256': case['wasm_sha256'], 'operation': case['operation'],
                          'memory64': case['memory64'], 'command': command,
                          'exit': completed.returncode, 'seconds': time.monotonic() - started,
                          'stdout': completed.stdout, 'stderr': completed.stderr,
                          'same_instance_trap_checks': 8 * dispatches,
                          'observer_negative_controls': 2 * dispatches,
                          'fault_readback_bytes': (4 * 2 * 65536 + 4 * 2 * 131072) * dispatches,
                          'post_grow_old_boundary_positive':
                              (2 if case['operation'] == 'copy' else 1) * dispatches}
                trap_lines = re.findall(
                    r'^PASS same-instance memory\.(copy|init) (\S+) generation=(\d+) '
                    r'memory(32|64) checked-bytes=(\d+)$', completed.stdout,
                    flags=re.MULTILINE)
                scenario_names = ('source-partial', 'destination-partial',
                                  'source-zero-beyond', 'destination-zero-beyond') if \
                    case['operation'] == 'copy' else (
                        'data-source-partial', 'destination-partial',
                        'data-source-zero-beyond', 'destination-zero-beyond')
                expected_lines = Counter(
                    (case['operation'], scenario, str(generation),
                     '64' if case['memory64'] else '32',
                     str(2 * 65536 * (generation + 1)))
                    for generation in range(2) for scenario in scenario_names
                    for _ in range(dispatches))
                if (completed.stdout.count('READBACK-OK') != result['same_instance_trap_checks'] or
                        Counter(trap_lines) != expected_lines or
                        completed.stdout.count('PASS observer-negative-controls=2') != dispatches or
                        completed.stdout.count('PASS post-grow-old-boundary') !=
                            result['post_grow_old_boundary_positive']):
                    result['exit'] = -1
                    result['readback_marker_failure'] = True
                results.append(result)
                write_json(output / 'results.json', results)
                if result['exit']:
                    raise RuntimeError(f'bulk fault-first failure: {result}')

    after_manifest = output / 'source-manifest-after.json'
    after_id = subprocess.check_output(['python3', str(fingerprint), str(source),
                                        str(after_manifest)], text=True).strip()
    after = cgroup()
    if after_id != source_id or any(event(after, name) != event(before, name)
                                    for name in ('oom', 'oom_kill')):
        raise RuntimeError('source changed or cgroup OOM during qualification')
    if len(results) != 16 or sum(r['same_instance_trap_checks'] for r in results) != 192:
        raise RuntimeError('missing int/JIT, policy, copy/init, memory32/64 checks')
    input_files = [Path(__file__), generator, source / int_file, source / jit_file,
                   source / common_file,
                   source / 'test/0013.uwvm_int/strict/uwvm_int_translate_strict_common.h',
                   Path(args.compiler)]
    summary = {'source_id': source_id,
               'normative_execution_rule':
                   'https://webassembly.github.io/spec/core/exec/instructions.html',
               'source_before_sha256': digest(before_manifest),
               'source_after_sha256': digest(after_manifest),
               'normal_build_json': str(args.normal_build_json.resolve()),
               'normal_build_json_sha256': digest(args.normal_build_json),
               'normal_cli_sha256': digest(cli),
               'normal_runtime_object_sha256': digest(runtime),
               'binaries': binaries, 'test_inputs_sha256':
                   {str(path.resolve()): digest(path.resolve()) for path in input_files},
               'fixture_reference_sha256': digest(reference_path),
               'wasm_oracles': reference,
               'cgroup_before': before, 'cgroup_after': after,
               'runs': len(results),
               'same_instance_trap_checks': sum(r['same_instance_trap_checks'] for r in results),
               'observer_negative_controls': sum(r['observer_negative_controls'] for r in results),
               'fault_readback_bytes': sum(r['fault_readback_bytes'] for r in results),
               'post_grow_old_boundary_positive':
                   sum(r['post_grow_old_boundary_positive'] for r in results),
               'scope': 'memory.copy/init both bounds, cross memory, memory32/64, before/after grow; full int and full JIT instruction/unwind'}
    write_json(output / 'summary.json', summary)
    print(f"PASS {len(results)} runs, {summary['same_instance_trap_checks']} "
          f"same-instance traps, {summary['fault_readback_bytes']} checked bytes")


if __name__ == '__main__':
    main()

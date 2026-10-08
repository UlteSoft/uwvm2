#!/usr/bin/env python3
"""Run the final-source interpreter fusion regressions at every combine/delay setting.

This is a test-only, sequential O3 build. The Core 3 source and normal VM
product are checked before and after; every strict binary is compiled from the
same immutable source and exercised under all four supported interpreter ABIs.
"""

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import time


TESTS = (
    'test/0013.uwvm_int/strict/conbine/'
    'uwvm_int_translate_conbine_i32_rotl_localtee_brif_regress_strict.cc',
    'test/0013.uwvm_int/strict/call/'
    'uwvm_int_translate_call_fuse_ret_i64_f32_f64_strict.cc',
)
ABIS = ('byref', 'tail-min', 'tail-sysv', 'tail-aapcs64')
COMBINE = {
    'none': (),
    'soft': ('UWVM_ENABLE_UWVM_INT_COMBINE_OPS',),
    'heavy': ('UWVM_ENABLE_UWVM_INT_COMBINE_OPS',
              'UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS'),
    'extra': ('UWVM_ENABLE_UWVM_INT_COMBINE_OPS',
              'UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS',
              'UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS'),
}
DELAY = {
    'none': (),
    'soft': ('UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT',),
    'heavy': ('UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT',
              'UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY'),
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(path: Path, value: object) -> None:
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def cgroup() -> dict[str, str]:
    root = Path('/sys/fs/cgroup')
    result = {name: (root / name).read_text().strip() for name in
              ('memory.max', 'memory.swap.max', 'cpuset.cpus.effective',
               'memory.events')}
    expected = ('68719476736', '0', '0,2,4,6,16-31')
    if tuple(result[name] for name in ('memory.max', 'memory.swap.max',
                                      'cpuset.cpus.effective')) != expected:
        raise RuntimeError(f'wrong Linux build/test cgroup: {result}')
    return result


def events(cg: dict[str, str]) -> Counter[str]:
    return Counter({name: int(value) for name, value in
                    (line.split() for line in cg['memory.events'].splitlines())})


def fingerprint(source: Path, manifest: Path) -> str:
    script = source / 'tools/ci/wasm3_source_fingerprint.py'
    return subprocess.check_output(['python3', str(script), str(source),
                                    str(manifest)], text=True).strip()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--normal-build-json', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', type=Path,
                        default=Path('/toolchain/bin/clang++'))
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    before = cgroup()
    source_id = fingerprint(source, output / 'source-before.json')
    build_path = args.normal_build_json.resolve(strict=True)
    build = json.loads(build_path.read_text())
    runtime = Path(build['runtime_command'][-1]).resolve(strict=True)
    product = Path(build['cli_command'][-1]).resolve(strict=True)
    if (build['source_id'] != source_id or
            Path(build['source']).resolve(strict=True) != source or
            sha(runtime) != build['runtime_object_sha256'] or
            sha(product) != build['binary_sha256']):
        raise RuntimeError('normal O3 product is not source-matched')
    # Preserve the clang++ driver name.  Resolving its clang-23 symlink turns
    # an implicit C++ link into a C link and omits libc++/libc++abi.
    compiler = args.compiler.absolute()
    if not compiler.is_file():
        raise RuntimeError(f'missing C++ compiler: {compiler}')
    flags = ['-std=c++26', '-stdlib=libc++', '-fuse-ld=lld',
             '-rtlib=compiler-rt', '-unwindlib=libunwind', '-O3', '-g0',
             '-Wno-undefined-inline', '-DUWVM=2', '-DUWVM_USE_UWVM_INT',
             '-DUWVM_DISABLE_JIT', '-DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1',
             f'-DUWVM2_BUILD_SOURCE_ID=u8"{source_id}"',
             '-I', 'src', '-I', 'third-parties/bizwen/include',
             '-I', 'third-parties/fast_io/include',
             '-I', 'third-parties/boost_unordered/include']
    records: list[dict] = []
    for combine, combine_macros in COMBINE.items():
        for delay, delay_macros in DELAY.items():
            tuning = [f'-D{macro}' for macro in (*combine_macros,
                                                *delay_macros)]
            for test_relative in TESTS:
                test = source / test_relative
                name = f'{test.stem}-{combine}-{delay}'
                binary = output / name
                command = [str(compiler), *flags, *tuning,
                           test_relative, '-o', str(binary)]
                started = time.monotonic()
                with (output / f'{name}.build.log').open('w') as stream:
                    compiled = subprocess.run(command, cwd=source, stdout=stream,
                                              stderr=subprocess.STDOUT,
                                              timeout=1800)
                record = {'source_id': source_id,
                          'test': test_relative, 'test_sha256': sha(test),
                          'combine': combine, 'delay': delay,
                          'compile_command': command,
                          'compile_exit': compiled.returncode,
                          'compile_seconds': time.monotonic() - started}
                records.append(record)
                save(output / 'runs.json', records)
                if compiled.returncode:
                    raise RuntimeError(f'{name} compile failed; see build log')
                record['binary_sha256'] = sha(binary)
                for abi in ABIS:
                    environment = dict(os.environ,
                                       UWVM2TEST_ABI_MODES=abi,
                                       UWVM2TEST_MATRIX_LEVEL='default')
                    completed = subprocess.run([str(binary)], cwd=source,
                                               env=environment, capture_output=True,
                                               text=True, timeout=120)
                    record.setdefault('executions', []).append({
                        'source_id': source_id,
                        'binary_sha256': record['binary_sha256'],
                        'test_sha256': record['test_sha256'],
                        'abi': abi, 'exit': completed.returncode,
                        'stdout': completed.stdout, 'stderr': completed.stderr})
                    save(output / 'runs.json', records)
                    if completed.returncode:
                        raise RuntimeError(f'{name}/{abi} failed')
    after_id = fingerprint(source, output / 'source-after.json')
    after = cgroup()
    if (after_id != source_id or sha(runtime) != build['runtime_object_sha256'] or
            sha(product) != build['binary_sha256'] or
            any(events(after)[key] != events(before)[key]
                for key in ('oom', 'oom_kill'))):
        raise RuntimeError('source/product changed or cgroup OOM during matrix')
    if len(records) != 24 or sum(len(record['executions']) for record in records) != 96:
        raise RuntimeError('incomplete strict fusion matrix')
    test_inputs = [source / relative for relative in TESTS]
    test_inputs += [source / 'test/0013.uwvm_int/strict/'
                    'uwvm_int_translate_strict_common.h',
                    source / 'tools/ci/run_wasm3_final_strict_fusion_matrix.py',
                    compiler]
    matrix = [{'source_id': execution['source_id'],
               'test': record['test'], 'test_sha256': execution['test_sha256'],
               'combine': record['combine'], 'delay': record['delay'],
               'abi': execution['abi'],
               'binary_sha256': execution['binary_sha256'],
               'exit': execution['exit']}
              for record in records for execution in record['executions']]
    summary = {'passed': True, 'source_id': source_id,
               'normal_build_json': str(build_path),
               'normal_build_json_sha256': sha(build_path),
               'normal_runtime_sha256': sha(runtime),
               'normal_product_sha256': sha(product),
               'source_before_sha256': sha(output / 'source-before.json'),
               'source_after_sha256': sha(output / 'source-after.json'),
               'test_inputs_sha256': {str(path): sha(path) for path in test_inputs},
               'runs_json_sha256': sha(output / 'runs.json'),
               'matrix': matrix,
               'compile_count': len(records),
               'execution_count': sum(len(record['executions']) for record in records),
               'combine': list(COMBINE), 'delay': list(DELAY), 'abis': ABIS,
               'cgroup_before': before, 'cgroup_after': after}
    save(output / 'summary.json', summary)
    print('PASS 24 O3 strict fusion binaries and 96 ABI executions')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Compile/run real 32/64-bit WASIp1 get units in the existing Linux cgroup."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

from run_entry_edges import admission


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--test-source-dir', type=Path,
                        default=Path(__file__).resolve().parents[2] / '0008.imported/wasi/wasip1/func')
    parser.add_argument('--header-overlay', type=Path)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--expected-cgroup', required=True)
    args = parser.parse_args()
    before = admission(args.expected_cgroup)
    args.out.mkdir(parents=True, exist_ok=False)
    sdk_lib = args.sdk / 'lib/x86_64-unknown-linux-gnu'
    environment = dict(os.environ, LD_LIBRARY_PATH=str(sdk_lib) + ':' + str(args.sdk / 'lib'))
    compiler = args.sdk / 'bin/clang++'
    report = {'passed': False, 'admission_before': before, 'compiler': str(compiler),
              'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(), 'runs': []}
    for case in ['args_get', 'environ_get', 'args_get_wasm64', 'environ_get_wasm64']:
        source = args.test_source_dir / (case + '.cc')
        target = args.out / case
        command = [str(compiler), '-std=c++26', '-O2', '-g', '-stdlib=libc++', '-fuse-ld=lld',
                   '-rtlib=compiler-rt', '-unwindlib=libunwind', '-DUWVM=2',
                   '-DUWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64', '-L' + str(sdk_lib),
                   '-Wl,-rpath,' + str(sdk_lib), '-Wno-undefined-inline']
        if args.header_overlay:
            command += ['-I' + str(args.header_overlay)]
        for include in ['src', 'third-parties/fast_io/include', 'third-parties/bizwen/include', 'third-parties/boost_unordered/include']:
            command += ['-I' + str(args.source_root / include)]
        # An overlay holds only the four under-test headers; their quoted
        # base.h dependency still comes from the selected repository snapshot.
        command += ['-I' + str(args.source_root / 'src/uwvm2/imported/wasi/wasip1/func')]
        command += [str(source), '-pthread', '-o', str(target)]
        row = {'case': case, 'passed': False, 'compile_argv': command,
               'test_source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
               'width_bits': 64 if case.endswith('_wasm64') else 32,
               'counts': [0, 1, 2, 3] if case == 'environ_get' else [0, 1, 2],
               'layouts': ['table-adjacent-canary', 'table-exact-memory-end'],
               'string_nul_and_utf8_checked': True, 'string_tail_canary_checked': True}
        header_root = args.header_overlay or (args.source_root / 'src')
        header = header_root / 'uwvm2/imported/wasi/wasip1/func' / (case + '.h')
        row['tested_header_sha256'] = hashlib.sha256(header.read_bytes()).hexdigest()
        start = time.monotonic()
        built = subprocess.run(command, env=environment, capture_output=True, text=True, timeout=120)
        row.update(compile_exit=built.returncode, compile_stderr=built.stderr,
                   compile_seconds=time.monotonic() - start)
        if built.returncode == 0:
            executed = subprocess.run([str(target)], env=environment, capture_output=True, text=True, timeout=20)
            row.update(run_exit=executed.returncode, stdout=executed.stdout, stderr=executed.stderr,
                       binary_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
                       passed=executed.returncode == 0 and executed.stdout == case + ' exact-count vector contract: PASS\n')
        report['runs'].append(row)
        print(json.dumps({'case': case, 'passed': row['passed'], 'compile_exit': row['compile_exit'],
                          'run_exit': row.get('run_exit'), 'error': row.get('compile_stderr', '')[-1200:]}), flush=True)
        (args.out / 'report.json').write_text(json.dumps(report, indent=2))
    report['admission_after'] = admission(args.expected_cgroup)
    assert report['admission_before']['memory_events'] == report['admission_after']['memory_events']
    report['passed'] = all(row['passed'] for row in report['runs'])
    (args.out / 'report.json').write_text(json.dumps(report, indent=2))
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()

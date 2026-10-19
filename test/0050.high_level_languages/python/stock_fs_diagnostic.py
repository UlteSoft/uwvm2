#!/usr/bin/env python3
"""One authorized stock startup trace; no source, bundle or rights change."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys

import linux_control
from stock_stdlib import inventory

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from run_matrix import argv_for


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arena', type=Path, default=Path('/tmp/high-level-20261017'))
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--resolve-host', action='store_true')
    parser.add_argument('--strace', action='store_true', help='Capture this owned target file calls and return values only')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    read_guard = linux_control.guard
    before = read_guard(args.out)
    os.sched_setaffinity(0, {4})
    stops = []

    def priority_guard(path):
        state = read_guard(path)
        if not stops and state['anon'] >= 50 << 30:
            stops.append({'shared_anon': state['anon'], 'reason': 'parent DBG priority >=50GiB'})
            raise RuntimeError('filesystem diagnostic stopped for parent DBG memory priority')
        return state

    linux_control.guard = priority_guard
    previous = args.arena / 'python/stock-stdlib-r1'
    baseline = json.loads((previous / 'report.json').read_text())
    assets = json.loads((previous / 'stdlib-assets.json').read_text())
    given = Path(baseline['preopens'][0]['host'])
    resolved = given.resolve()
    host_path = resolved if args.resolve_host else given
    binary = args.arena / 'products/uwvm2/uwvm'
    wasm = Path(baseline['stock_wasm'])
    digest = linux_control.digest
    assert digest(wasm) == baseline['stock_wasm_sha256']
    assert inventory(given) == assets['files']
    trace = args.out.resolve() / 'wasip1-trace.log'
    record = {'diagnostic_only': True, 'collection_completed': False, 'before': before,
              'helper_sha256': digest(Path(__file__)), 'stops': stops,
              'binary': str(binary), 'binary_sha256_before': digest(binary),
              'wasm': str(wasm), 'wasm_sha256_before': digest(wasm),
              'host_bundle_given': str(given), 'host_bundle_resolved': str(resolved),
              'host_bundle_used': str(host_path), 'guest_mount': '/python',
              'host_path_resolution_only': args.resolve_host,
              'trace': str(trace), 'source_files': []}
    source_root = args.arena / 'uwvm2-ros'
    for relative in [
        'src/uwvm2/uwvm/cmdline/callback/wasip1_global_mount_dir.h',
        'src/uwvm2/uwvm/imported/wasi/wasip1/init/init_env.h']:
        path = source_root / relative
        target = args.out / 'frozen-source' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        record['source_files'].append({'path': str(path), 'sha256': digest(path),
                                       'copy': str(target)})
    report = args.out / 'report.json'
    def save():
        report.write_text(json.dumps(record, indent=2) + '\n')
    save()
    try:
        linux_control.invoke([binary, '--help', 'wasi'], args.out / 'help', timeout=30)
        help_text = (args.out / 'help/stdout.log').read_bytes() + (args.out / 'help/stderr.log').read_bytes()
        assert b'--wasip1-trace' in help_text
        record['actual_binary_help_supports_trace'] = True
        env = baseline['guest_environment']
        load = ['--wasip1-noinherit-system-environment', '--wasip1-mount-dir', '/python', str(host_path),
                *[token for pair in env.items() for token in ['--wasip1-add-or-replace-environment', *pair]],
                '--wasip1-trace', 'file', str(trace)]
        case = {'wasm': str(wasm), 'args': ['-S', '-B', '-X', 'utf8', '-c',
                                         'import sys; print(sys.path)'], 'load_args': load}
        argv = argv_for(binary, 'uwvm2', case, False)
        if args.strace:
            tracer = shutil.which('strace')
            assert tracer, 'strace is not installed; do not install it implicitly'
            record['strace_binary'] = tracer
            record['strace_binary_sha256'] = digest(Path(tracer))
            record['strace_log'] = str(args.out.resolve() / 'strace.log')
            argv = [tracer, '-f', '-qq', '-s', '256', '-yy', '-e',
                    'trace=%file,read,lseek,getdents64', '-o', record['strace_log'], *argv]
        record['argv'] = argv
        save()
        try:
            linux_control.invoke(argv, args.out / 'run', timeout=180, expected_exit=1)
        except Exception as error:
            record['run_error'] = repr(error)
        process = json.loads((args.out / 'run/receipt.json').read_text())
        record['exit'] = process.get('exit')
        record['ownership'] = process['ownership']
        record['retired'] = process['retired']
        record['stdout_sha256'] = digest(args.out / 'run/stdout.log')
        record['stderr_sha256'] = digest(args.out / 'run/stderr.log')
        record['trace_exists'] = trace.exists()
        if trace.exists():
            record['trace_sha256'] = digest(trace)
            record['trace_bytes'] = trace.stat().st_size
        record['collection_completed'] = bool(process['retired'] and process['ownership']['closed']
            and all(not m['pidfd_retained'] for m in process['ownership']['members']))
    except BaseException as error:
        record['error'] = repr(error)
    finally:
        record['binary_sha256_after'] = digest(binary)
        record['wasm_sha256_after'] = digest(wasm)
        record['bundle_unchanged'] = inventory(given) == assets['files']
        record['after'] = read_guard(args.out)
        save()


if __name__ == '__main__':
    main()

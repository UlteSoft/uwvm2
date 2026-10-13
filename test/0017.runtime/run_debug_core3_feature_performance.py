#!/usr/bin/env python3
"""Core 3 semantic, real debugger interaction, and paired timing qualification.

Only run inside the verified SSH Linux cgroup. Same Wasm bytes in all modes;
kernel CLOCK_MONOTONIC excludes compilation, startup and debugger attachment.
Never infer a checksum from another engine. Unsupported profiles and failed
inputs remain visible and cannot produce a complete-coverage result.
"""
from debug_sampling_safety import durable_write, sync_artifacts
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import statistics
import subprocess
import time

from run_debug_breakpoint_benchmark import exports
from run_debug_run_cli import Console
from run_debug_wasmtime_comparison import finish_batch

PROMPT = rb'(?:^|\n)\(uwvm-debug\) $'
MODES = ('uwvm-ordinary', 'uwvm-none', 'uwvm-cold', 'uwvm-disabled',
         'wasmtime-ordinary', 'wasmtime-gdb', 'wasmtime-guest')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def metrics(output, checksum):
    times = re.findall(rb'Kernel ns: (\d+)', output)
    sums = re.findall(rb'Checksum: (\d+)', output)
    assert len(times) == len(sums) == 1, output[-3000:]
    assert int(sums[0]) == checksum, (checksum, sums)
    return {'kernel_ns': int(times[0]), 'checksum': int(sums[0])}


def run_logged(argv, out, timeout):
    try:
        process = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        out.write_bytes(error.stdout or b'')
        raise
    out.write_bytes(process.stdout)
    return process


def command(console, text, timeout):
    at = len(console.log)
    os.write(console.fd, text.encode() + b'\n')
    reply = console.wait(lambda b: re.search(PROMPT, b), at, timeout)
    assert not re.search(rb'error:|Undefined command|not valid', reply), reply[-3000:]
    return reply


def uwvm_run(binary, wasm, row, mode, out, *, ros, timeout, cgroup):
    debug = mode != 'uwvm-ordinary'
    flags = ['-Raot'] if ros else ['-Rcm', 'full', '-Rcc', 'jit']
    argv = [str(binary), *(['-Rdbg'] if debug else []), *flags,
            '-Rct', '0', '-Rllvm-cache-path', 'disable', '-Rclog', 'err',
            *['-WFE-' + feature for feature in row['required_features']], '--run', str(wasm)]
    begin = time.perf_counter()
    if not debug:
        process = run_logged(argv, out, timeout)
        assert process.returncode == 0, process.stdout[-3000:]
        return {**metrics(process.stdout, row['expected_checksum']), 'argv': argv,
                'process_s': time.perf_counter() - begin, 'startup_s': None}
    console = Console(argv)
    try:
        console.wait(lambda b: re.search(PROMPT, b), 0, timeout)
        assert Path(f'/proc/{console.pid}/cgroup').read_text().strip() == cgroup
        assert os.sched_getaffinity(console.pid) == os.sched_getaffinity(0)
        startup = time.perf_counter() - begin
        command(console, 'startw', timeout)
        symbols = exports(wasm)
        if mode in ('uwvm-cold', 'uwvm-disabled'):
            reply = command(console, f'bw 0 {symbols["probe_cold"]} 0', timeout)
            assert b'registered' in reply or b'Breakpoint' in reply, reply
            if mode == 'uwvm-disabled': command(console, 'disable', timeout)
        if mode == 'uwvm-qualify':
            command(console, f'bw 0 {symbols["kernel"]} 0', timeout)
            stopped = command(console, 'c', timeout)
            assert b'stopped: breakpoint' in stopped and \
                f'function={symbols["kernel"]} byte-offset=0 '.encode() in stopped, stopped
            values = command(console, 'locals wasm 1 0 0 1', timeout)
            assert f'local 0 i32 = {row["iterations"]}'.encode() in values, values
            command(console, 'operands 1 0 0 16', timeout)
            command(console, 'disable', timeout)
            command(console, 'nw', timeout)
            native = command(console, 'ni', timeout)
            assert b'native-pc=' in native, native
            command(console, 'nw', timeout)
            command(console, 'trace wasm on', timeout)
        reply = command(console, 'c', timeout)
        assert b'Program exited' in reply, reply[-3000:]
        measured = metrics(reply, row['expected_checksum'])
        if mode in ('uwvm-cold', 'uwvm-disabled'):
            assert b'hits=0' in command(console, 'info breakpoints', timeout)
        if mode == 'uwvm-qualify':
            trace = command(console, 'trace wasm read 0 128', timeout)
            assert b'wasm-event ' in trace, trace[-3000:]
        policies = [[x.decode() for x in value] for value in re.findall(
            rb'optimize-start[^\n]* policy=([^ ]+) pipeline=([^ ]+) codegen_opt=([^ ]+)', console.log)]
        assert policies and all(value == ['session:debug', 'debug-local-ssa', 'less'] for value in policies), policies
        console.finish()
        return {**measured, 'argv': argv, 'policies': policies,
                'process_s': time.perf_counter() - begin, 'startup_s': startup,
                'timing_qualified': mode != 'uwvm-qualify'}
    finally:
        out.write_bytes(console.log)
        console.close()


def wasmtime_run(binary, lldb, wasm, row, mode, out, timeout):
    flags = ['run', '-C', 'cache=n', '-O', 'opt-level=2', '-W', 'memory64=y']
    if row['name'] == 'deterministic_profile':
        flags += ['-W', 'nan-canonicalization=y', '-W', 'relaxed-simd-deterministic=y']
    if mode == 'wasmtime-gdb': flags += ['-D', 'debug-info']
    argv = [str(binary), *flags, str(wasm)]
    begin, startup = time.perf_counter(), None
    if mode == 'wasmtime-guest':
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            port = sock.getsockname()[1]
        server_log = out.with_suffix('.server.bin')
        with server_log.open('xb') as stream:
            server_argv = [str(binary), *flags, '-g', str(port), str(wasm)]
            server = subprocess.Popen(server_argv, stdout=stream, stderr=subprocess.STDOUT)
            console = None
            try:
                deadline = time.monotonic() + timeout
                while b'Debugger listening' not in server_log.read_bytes():
                    assert server.poll() is None, server_log.read_bytes()[-3000:]
                    assert time.monotonic() < deadline, 'guest debugger ready deadline'
                    time.sleep(.02)
                argv = [str(lldb), '--no-lldbinit', '--batch',
                        '-o', f'log enable gdb-remote packets -f {out.with_suffix(".packets")}',
                        '-o', f'process connect --plugin wasm connect://127.0.0.1:{port}',
                        '-o', 'process status', '-o', 'thread backtrace', '-o', 'continue']
                console = Console(argv)
                console.wait(lambda b: b'Process 1 stopped' in b, 0, timeout)
                startup = time.perf_counter() - begin
                console.wait(lambda b: b'exited with status = 0' in b, 0, timeout)
                finish_batch(console)
                server.wait(timeout=10)
                assert server.returncode == 0
                output = bytes(console.log) + server_log.read_bytes()
                packets = out.with_suffix('.packets').read_bytes()
                assert b'qWasmCallStack' in packets, 'must exercise the Wasm plugin, not a native LLDB session'
            finally:
                if console is not None:
                    out.write_bytes(console.log)
                    console.close()
                if server.poll() is None: server.terminate()
                try: server.wait(timeout=5)
                except subprocess.TimeoutExpired: server.kill(); server.wait()
    elif mode == 'wasmtime-gdb':
        argv = ['/usr/bin/gdb', '-q', '-nx', '--batch', '-ex', 'set pagination off',
                '-ex', 'run', '--args', *argv]
        process = run_logged(argv, out, timeout)
        output = process.stdout
        assert process.returncode == 0 and b'exited normally' in output, output[-3000:]
    else:
        process = run_logged(argv, out, timeout)
        output = process.stdout
        assert process.returncode == 0, output[-3000:]
    return {**metrics(output, row['expected_checksum']), 'argv': argv,
            'process_s': time.perf_counter() - begin, 'startup_s': startup,
            **({'server_argv': server_argv} if mode == 'wasmtime-guest' else {})}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'uwvm', 'wasmtime', 'lldb', 'fixtures', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--cpu', type=int, default=20)
    parser.add_argument('--repeats', type=int, default=5)
    parser.add_argument('--timeout', type=int, default=180)
    parser.add_argument('--only', action='append')
    parser.add_argument('--mode', action='append', choices=MODES)
    parser.add_argument('--qualify-only', action='store_true')
    args = parser.parse_args()
    if not 1 <= args.repeats <= 10 or not 1 <= args.timeout <= 600:
        parser.error('require bounded repeats and timeout')
    root = args.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    assert os.sched_getaffinity(0) == {args.cpu}, 'pin this guarded runner before launch'
    args.out.mkdir(parents=True, exist_ok=False)
    os.environ.update(TERM='dumb', NO_COLOR='1', DEBUGINFOD_URLS='')
    rows = json.loads((args.fixtures / 'manifest.json').read_text())
    if args.only:
        assert set(args.only) <= {row['name'] for row in rows}
        rows = [row for row in rows if row['name'] in args.only]
    files = [args.uwvm, args.wasmtime, args.lldb, Path(__file__).resolve(),
             root / 'test/0017.runtime/debug_sampling_safety.py',
             args.fixtures / 'manifest.json', root / 'test/0017.runtime/run_debug_run_cli.py',
             root / 'test/0017.runtime/run_debug_wasmtime_comparison.py',
             root / 'test/0017.runtime/run_debug_breakpoint_benchmark.py',
             root / 'tools/ci/require_wasm3_test_cgroup.sh', Path('/usr/bin/gdb')]
    if args.baseline: files.append(args.baseline)
    files += [args.fixtures / (row['name'] + suffix) for row in rows for suffix in ('.wat', '.wasm')
              if (args.fixtures / (row['name'] + suffix)).exists()]
    immutable = {str(path.resolve()): digest(path) for path in files}
    result = {'passed': False, 'coverage_complete': False, 'timing_matrix_complete': False,
              'parity_achieved': False, 'median_band_matched': False, 'variance_review': 'pending',
              'cgroup': Path('/proc/self/cgroup').read_text().strip(),
              'affinity': sorted(os.sched_getaffinity(0)), 'fixtures': rows,
              'immutable_before': immutable, 'qualification': [], 'samples': [], 'failures': [],
              'notes': ['GDB/native DWARF has different capabilities from the Wasm/LLDB plugin.',
                        'Qualification with visible stops and tracing is excluded from throughput.',
                        'Kernel clocks exclude JIT/startup/attach; process_s reports them separately.',
                        'The deterministic execution profile has no UWVM CLI selector in the inspected source.']}
    def save(): durable_write(args.out / 'result.json', json.dumps(result, indent=2) + '\n')
    for row in rows:
        name, wasm = row['name'], (args.fixtures / (row['name'] + '.wasm')).resolve()
        if row['validation'] != 'passed':
            result['failures'].append({'name': name, 'phase': 'validation', 'passed': False,
                                       'error': 'The official parser/validator did not qualify this input.'})
            save()
            continue
        assert digest(wasm) == row['wasm_sha256']
        qualified = True
        qualifiers = [('after', args.uwvm.resolve(), mode)
                      for mode in ('wasmtime-ordinary', 'uwvm-ordinary', 'uwvm-qualify')]
        if args.baseline:
            qualifiers += [('before', args.baseline.resolve(), mode) for mode in ('uwvm-ordinary', 'uwvm-qualify')]
        for version, binary, mode in qualifiers:
            try:
                if mode.startswith('uwvm'):
                    measured = uwvm_run(binary, wasm, row, mode, args.out / f'{name}-{version}-{mode}-qualification.bin',
                                        ros=args.ros, timeout=args.timeout, cgroup=result['cgroup'])
                else:
                    measured = wasmtime_run(args.wasmtime.resolve(), args.lldb.resolve(), wasm, row, mode,
                                            args.out / f'{name}-{version}-{mode}-qualification.bin', args.timeout)
                result['qualification'].append({'name': name, 'mode': mode, 'version': version, 'passed': True, **measured})
            except Exception as error:
                qualified = False
                result['failures'].append({'name': name, 'phase': 'qualification', 'mode': mode, 'version': version,
                                           'error': str(error), 'passed': False})
            save()
        if name == 'deterministic_profile':
            qualified = False
            result['failures'].append({'name': name, 'phase': 'profile-capability', 'passed': False,
                                       'error': 'No explicit UWVM deterministic execution profile selector; passing finite probes cannot prove profile conformance.'})
            save()
        if not qualified or args.qualify_only: continue
        targets = [(mode, 'after', args.uwvm.resolve()) for mode in args.mode or MODES]
        if args.baseline:
            targets += [(mode, 'before', args.baseline.resolve()) for mode in args.mode or MODES if mode.startswith('uwvm')]
        for repeat in range(args.repeats):
            for mode, version, binary in targets if repeat % 2 == 0 else list(reversed(targets)):
                out = args.out / f'{name}-{version}-{mode}-{repeat}.bin'
                try:
                    measured = uwvm_run(binary, wasm, row, mode, out, ros=args.ros, timeout=args.timeout, cgroup=result['cgroup']) \
                        if mode.startswith('uwvm') else wasmtime_run(args.wasmtime.resolve(), args.lldb.resolve(), wasm, row, mode, out, args.timeout)
                    result['samples'].append({'name': name, 'mode': mode, 'version': version, 'repeat': repeat,
                                              'binary_sha256': digest(binary) if mode.startswith('uwvm') else digest(args.wasmtime), **measured})
                except Exception as error:
                    result['failures'].append({'name': name, 'mode': mode, 'version': version, 'repeat': repeat,
                                               'phase': 'timing', 'error': str(error), 'passed': False})
                save()
                print(json.dumps({'name': name, 'mode': mode, 'version': version, 'repeat': repeat}), flush=True)
    groups = {(r['name'], r['version'], r['mode']) for r in result['samples']}
    result['medians_ns'] = {'/'.join(group): statistics.median(r['kernel_ns'] for r in result['samples']
        if (r['name'], r['version'], r['mode']) == group) for group in sorted(groups)}
    result['sample_ranges_ns'] = {'/'.join(group): {
        'count': len(values), 'min': min(values), 'max': max(values)}
        for group in sorted(groups)
        for values in [[r['kernel_ns'] for r in result['samples']
                        if (r['name'], r['version'], r['mode']) == group]]}
    expected_groups = {(row['name'], 'after', mode) for row in rows for mode in args.mode or MODES}
    if args.baseline:
        expected_groups |= {(row['name'], 'before', mode) for row in rows
                            for mode in args.mode or MODES if mode.startswith('uwvm')}
    result['timing_matrix_complete'] = not args.qualify_only and groups == expected_groups and \
        all(value['count'] == args.repeats for value in result['sample_ranges_ns'].values())
    result['comparison'] = []
    for row in rows:
        prefix, medians = row['name'] + '/after/', result['medians_ns']
        keys = [prefix + mode for mode in ('uwvm-ordinary', 'uwvm-none', 'wasmtime-ordinary', 'wasmtime-guest')]
        if all(key in medians for key in keys):
            u, d, w, g = [medians[key] for key in keys]
            result['comparison'].append({'name': row['name'], 'uwvm_debug_over_ordinary': d / u,
                                         'wasmtime_guest_over_ordinary': g / w, 'uwvm_over_wasmtime_guest': d / g})
    result['immutable_after'] = {path: digest(Path(path)) for path in immutable}
    result['coverage_complete'] = not result['failures'] and len({row['feature'] for row in rows}) == 10
    result['passed'] = not result['failures'] and result['immutable_after'] == immutable
    # A 5% median band is only a measurement criterion. Raw samples must still
    # be assessed for variance before reporting statistically justified parity.
    result['median_band_matched'] = result['passed'] and result['timing_matrix_complete'] and \
        len(result['comparison']) == len(rows) and all(row['uwvm_over_wasmtime_guest'] <= 1.05 for row in result['comparison'])
    # Leave parity_achieved false until the raw paired distributions receive
    # a separate variance assessment; a median alone is not that evidence.
    sync_artifacts(args.out)
    save()
    return 0 if result['passed'] else 1


if __name__ == '__main__': raise SystemExit(main())

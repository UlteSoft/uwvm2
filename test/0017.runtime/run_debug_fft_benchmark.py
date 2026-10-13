#!/usr/bin/env python3
"""Compare the same FFT Wasm under ordinary full JIT and real PTY -Rdbg.

The fixture reports CLOCK_MONOTONIC time around only the FFT kernel and a
checksum. Compilation, command entry and output formatting stay outside it.
Run without competing builds/profilers; this is elapsed time, not PMU evidence.
"""
import argparse, hashlib, json, os, re, statistics, subprocess, time
from pathlib import Path
from run_debug_run_cli import Console


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm', 'wasm', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--timeout', type=int, default=180)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--trace-wasm', action='store_true', help='Request complete opcode trace before continuing from startw.')
    args = parser.parse_args()
    assert 1 <= args.repeats <= 10 and 1 <= args.timeout <= 600
    args.out.mkdir(parents=True, exist_ok=False)
    os.environ['TERM'] = 'dumb'
    os.environ['NO_COLOR'] = '1'
    wasm = args.wasm.resolve()
    result = {'wasm': str(wasm), 'wasm_sha256': hashlib.sha256(wasm.read_bytes()).hexdigest(),
              'timing': 'guest CLOCK_MONOTONIC kernel; no hardware counters', 'samples': []}
    binaries = [('after', args.uwvm.resolve())]
    if args.baseline:
        binaries.insert(0, ('before', args.baseline.resolve()))
    checksums = set()
    for label, binary in binaries:
        for mode in ('ordinary', 'debug'):
            for repeat in range(args.repeats):
                name = f'{label}-{mode}-{repeat}'
                flags = ['-Raot'] if args.ros else ['-Rcm', 'full', '-Rcc', 'jit']
                argv = [str(binary), *(['-Rdbg'] if mode == 'debug' else []), *flags,
                        '-Rct', '0', '-Rllvm-cache-path', 'disable', '-Rclog', 'err', '--run', str(wasm)]
                started = time.perf_counter()
                startup = None
                trace_sequence = None
                if mode == 'debug':
                    c = Console(argv)
                    try:
                        c.wait(lambda b: b'(uwvm-debug) ' in b, 0, 120)
                        startup = time.perf_counter() - started
                        if args.trace_wasm:
                            c.command('startw')
                            c.command('trace wasm on')
                        at = len(c.log)
                        os.write(c.fd, b'c\n' if args.trace_wasm else b'run\n')
                        output = c.wait(lambda b: b'Time: ' in b and re.search(
                            rb'(?:^|\n)\(uwvm-debug\) $', b), at, args.timeout)
                        elapsed = time.perf_counter() - started
                        trace = c.command('trace wasm read 0 128') if args.trace_wasm else b''
                        assert b'error:' not in trace, trace[-3000:]
                        if args.trace_wasm:
                            pages = re.findall(rb'wasm-trace page oldest=(\d+) newest=(\d+)', trace)
                            assert len(pages) == 1, trace[-3000:]
                            trace_sequence = list(map(int, pages[0]))
                            assert trace_sequence[1] - trace_sequence[0] == 511, trace_sequence
                            # Formatting the measured time takes value-dependent
                            # Wasm paths. Whole-process event counts may differ;
                            # exact opcode completeness uses the codegen fixture.
                            (args.out / (name + '.trace.bin')).write_bytes(trace)
                        c.finish()
                    finally:
                        (args.out / (name + '.bin')).write_bytes(c.log)
                        c.close()
                else:
                    child = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                           timeout=args.timeout, check=True)
                    elapsed = time.perf_counter() - started
                    output = child.stdout
                    (args.out / (name + '.bin')).write_bytes(output)
                times = re.findall(rb'Time: ([0-9.]+) ms', output)
                sums = re.findall(rb'Checksum: ([^\r\n]+)', output)
                assert len(times) == len(sums) == 1, output[-4000:]
                checksums.add(sums[0].decode())
                assert len(checksums) == 1, ('FFT result changed', checksums)
                row = {'version': label, 'mode': mode, 'repeat': repeat,
                       'kernel_ms': float(times[0]), 'process_to_result_s': elapsed,
                       'trace_wasm': args.trace_wasm and mode == 'debug',
                       'trace_oldest_newest': trace_sequence,
                       'startup_s': startup, 'checksum': sums[0].decode(), 'argv': argv,
                       'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                       'policies': [[v.decode() for v in row] for row in re.findall(
                           rb'optimize-start[^\n]* policy=([^ ]+) pipeline=([^ ]+) codegen_opt=([^ ]+)',
                           bytes(c.log) if mode == 'debug' else output)]}
                result['samples'].append(row)
                (args.out / 'result.json').write_text(json.dumps(result, indent=2))
                print(label, mode, repeat, row['kernel_ms'], 'ms', flush=True)
    result['medians_ms'] = {label: {mode: statistics.median(
        row['kernel_ms'] for row in result['samples'] if row['version'] == label and row['mode'] == mode)
        for mode in ('ordinary', 'debug')} for label, _ in binaries}
    result['passed'] = True
    (args.out / 'result.json').write_text(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

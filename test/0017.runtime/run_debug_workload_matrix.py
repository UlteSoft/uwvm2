#!/usr/bin/env python3
"""Paired multi-kernel debugger timing using real PTYs and guest monotonic clocks.

No builds or profilers may compete with this run. GDB/native and uwvm/Wasm
use their own ordinary baselines: identical algorithms do not imply identical
code generation. Tiny trace fixtures qualify correctness, not kernel speed.
"""
import argparse, hashlib, json, os, platform, re, statistics, subprocess, time
from pathlib import Path
from run_debug_run_cli import Console
from run_debug_breakpoint_benchmark import exports


def metrics(output):
    ns = re.findall(rb'Kernel ns: (\d+)', output)
    checksum = re.findall(rb'Checksum: (\d+)', output)
    assert len(ns) == len(checksum) == 1, output[-4000:]
    return int(ns[0]), int(checksum[0])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ['uwvm', 'fixtures', 'out']: p.add_argument('--'+key, type=Path, required=True)
    p.add_argument('--baseline', type=Path)
    p.add_argument('--baseline-codegen', choices=['none', 'less'], default='none')
    p.add_argument('--baseline-pipeline', choices=['none', 'debug-local-ssa'], default='none')
    p.add_argument('--ros', action='store_true')
    p.add_argument('--native', action='store_true', help='Include ordinary native and GDB runs.')
    p.add_argument('--repeats', type=int, default=5)
    p.add_argument('--case', action='append', choices=['none', 'cold', 'disabled', 'trace'])
    p.add_argument('--only', action='append')
    a = p.parse_args(); assert 1 <= a.repeats <= 10
    a.out.mkdir(parents=True, exist_ok=False)
    os.environ.update(TERM='dumb', NO_COLOR='1', DEBUGINFOD_URLS='')
    manifest = json.loads((a.fixtures/'manifest.json').read_text())
    if a.only:
        manifest = [row for row in manifest if row['name'] in a.only]
        assert len(manifest) == len(set(a.only))
    binaries = [('after', a.uwvm.resolve())]
    if a.baseline: binaries.insert(0, ('before', a.baseline.resolve()))
    flags = ['-Raot'] if a.ros else ['-Rcm', 'full', '-Rcc', 'jit']
    result = {'passed':False, 'fixtures':manifest, 'samples':[],
              'cgroup':Path('/proc/self/cgroup').read_text().strip(),
              'affinity':sorted(os.sched_getaffinity(0)),
              'note':'Serial unprofiled guest CLOCK_MONOTONIC kernel time; startup and PTY outside kernel except visible stops. Each backend uses its own ordinary baseline.'}
    checksums = {}
    def save(): (a.out/'result.json').write_text(json.dumps(result, indent=2))
    for fixture in manifest:
        name = fixture['name']; wasm = (a.fixtures/(name+'.wasm')).resolve()
        assert hashlib.sha256(wasm.read_bytes()).hexdigest() == fixture['wasm_sha256']
        symbols = exports(wasm)
        modes = ['ordinary', *(a.case or ['none', 'cold', 'disabled'])]
        targets = [(label, binary, mode) for mode in modes for label, binary in binaries]
        if a.native: targets += [('native', (a.fixtures/(name+'-native')).resolve(), mode) for mode in ['ordinary', 'gdb']]
        # Alternate before/after at each repeat; avoid attributing time drift to
        # an optimization by collecting all old runs before all new runs.
        for repeat in range(a.repeats):
            ordered = targets if repeat % 2 == 0 else list(reversed(targets))
            for label, binary, mode in ordered:
                started = time.perf_counter(); startup = None; c = None; trace_range = None
                native = label == 'native'
                argv = [str(binary)] if native else [str(binary), *(['-Rdbg'] if mode != 'ordinary' else []), *flags,
                        '-Rct', '0', '-Rllvm-cache-path', 'disable', '-Rclog', 'err', '--run', str(wasm)]
                if mode == 'gdb': argv = ['/usr/bin/gdb', '-q', '-nx', '--args', str(binary)]
                stem = f'{name}-{label}-{mode}-{repeat}'
                if mode == 'ordinary':
                    reply = subprocess.check_output(argv, stderr=subprocess.STDOUT, timeout=180)
                    output = reply
                else:
                    c = Console(argv)
                    prompt = rb'\(gdb\) $' if native else rb'(?:^|\n)\(uwvm-debug\) $'
                    def ask(command):
                        at = len(c.log); os.write(c.fd, command.encode()+b'\n')
                        reply = c.wait(lambda b:re.search(prompt, b), at, 180)
                        assert not re.search(rb'error:|Undefined command|No symbol|not valid', reply), reply[-3000:]
                        return reply
                    try:
                        c.wait(lambda b:re.search(prompt, b), 0, 120)
                        assert Path(f'/proc/{c.pid}/cgroup').read_text().strip() == result['cgroup']
                        assert sorted(os.sched_getaffinity(c.pid)) == result['affinity']
                        startup = time.perf_counter()-started
                        if native:
                            ask('set pagination off'); ask('set confirm off'); reply = ask('run')
                            assert b'exited normally' in reply, reply[-3000:]
                        else:
                            ask('startw')
                            if mode in ['cold', 'disabled']:
                                bp = ask(f'bw 0 {symbols["probe_cold"]} 0')
                                assert re.search(rb'(?:Breakpoint|breakpoint) \d+', bp), bp
                                if mode == 'disabled': ask('disable')
                            if mode == 'trace': ask('trace wasm on')
                            reply = ask('c')
                            assert b'Program exited' in reply, reply[-3000:]
                            if mode in ['cold', 'disabled']:
                                assert b'hits=0' in ask('info breakpoints'), 'unused cold breakpoint was hit'
                            if mode == 'trace':
                                trace = ask('trace wasm read 0 128')
                                ranges = re.findall(rb'wasm-trace page oldest=(\d+) newest=(\d+)', trace)
                                assert len(ranges) == 1, trace[-3000:]
                                trace_range = list(map(int, ranges[0]))
                                assert trace_range[1]-trace_range[0] == 511, trace_range
                                (a.out/(stem+'.trace.bin')).write_bytes(trace)
                        c.finish(); output = bytes(c.log)
                    finally:
                        (a.out/(stem+'.bin')).write_bytes(c.log); c.close()
                ns, checksum = metrics(reply)
                policies = [[v.decode() for v in row] for row in re.findall(
                    rb'optimize-start[^\n]* policy=([^ ]+) pipeline=([^ ]+) codegen_opt=([^ ]+)', output)]
                if not native and mode != 'ordinary':
                    expected_codegen = a.baseline_codegen if label == 'before' else ('less' if (platform.system(), platform.machine()) == ('Linux', 'x86_64') else 'none')
                    expected_pipeline = a.baseline_pipeline if label == 'before' else ('debug-local-ssa' if (platform.system(), platform.machine()) == ('Linux', 'x86_64') else 'none')
                    assert policies and all(row == ['session:debug', expected_pipeline, expected_codegen] for row in policies), policies
                checksums.setdefault(name, checksum)
                assert checksums[name] == checksum, (name, checksums[name], checksum)
                if mode == 'ordinary': (a.out/(stem+'.bin')).write_bytes(output)
                row = {'name':name, 'version':label, 'mode':mode, 'repeat':repeat, 'kernel_ns':ns,
                       'checksum':checksum, 'process_s':time.perf_counter()-started, 'startup_s':startup,
                       'trace_oldest_newest':trace_range, 'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(), 'argv':argv,
                       'policies':policies}
                result['samples'].append(row); save(); print(name, label, mode, repeat, ns/1e6, 'ms', flush=True)
    result['medians_ms'] = {f'{name}/{label}/{mode}':statistics.median(row['kernel_ns']/1e6 for row in result['samples']
                            if (row['name'],row['version'],row['mode']) == (name,label,mode))
                            for name,label,mode in {(row['name'],row['version'],row['mode']) for row in result['samples']}}
    result['passed'] = True; save()


if __name__ == '__main__': main()

#!/usr/bin/env python3
"""Matched Core 3 hardware capture of Wasmtime's real Wasm/LLDB debugger.

Run only in the verified SSH Linux 64 GiB/no-swap cgroup. VTune launches
Wasmtime with collection paused, so JIT registration precedes execution.
Resume sampling at the actual start breakpoint, stop at profile_end, then
retire the guest outside collection. No samples are taken from LLDB itself.
"""
import argparse
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import re
import resource
import signal
import socket
import subprocess
import sys
import time

from run_debug_run_cli import Console
from run_debug_wasmtime_comparison import ask
from debug_sampling_safety import SamplingSession, durable_write, sampling_options, sync_artifacts


LAUNCHER = '''import json, os, sys
from pathlib import Path
cfg = json.loads(Path(sys.argv[1]).read_text())
assert Path('/proc/self/cgroup').read_text().strip() == cfg['cgroup']
os.sched_setaffinity(0, {cfg['cpu']})
assert os.sched_getaffinity(0) == {cfg['cpu']}
with open(cfg['server_log'], 'xb') as stream:
    os.dup2(stream.fileno(), 1)
    os.dup2(stream.fileno(), 2)
    stat = Path('/proc/self/stat').read_text().rsplit(')', 1)[1].split()
    receipt = {'pid': os.getpid(), 'start_ticks': stat[19],
               'cgroup': cfg['cgroup'], 'affinity': [cfg['cpu']]}
    path = Path(cfg['pid_receipt'])
    tmp = path.with_suffix('.tmp')
    tmp.write_text(json.dumps(receipt))
    tmp.rename(path)
    argv = ['/usr/bin/setpriv','--inh-caps=-all','--ambient-caps=-all',*cfg['argv']] if cfg['drop_capabilities'] else cfg['argv']
    os.execv(argv[0], argv)
'''


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def saved_run(argv, path, timeout=120):
    try:
        process = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        path.write_bytes(error.stdout or b'')
        raise
    path.write_bytes(process.stdout)
    assert process.returncode == 0, process.stdout[-3000:]
    return process.stdout


def lldb_command(console, text, timeout, safety=None):
    if safety is None:
        output = ask(console, text, prompt=rb'\(lldb\) $', timeout=timeout)
    else:
        offset = len(console.log)
        os.write(console.fd, text.encode() + b'\n')
        output = console.wait(safety.predicate(lambda data: re.search(rb'\(lldb\) $', data)), offset, min(timeout, 30))
    assert not re.search(rb'(?:^|\n)error:|invalid command', output), output[-3000:]
    return output



def collection_status(collector, logfile, since, safety, expected, timeout=5):
    # Hardware collectors can print -command status into the ORIGINAL
    # collector log while its requesting CLI exits successfully with no output.
    # Require an actual new status acknowledgement, never infer pause from argv.
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        safety.check()
        reply = logfile.read_bytes()[since:]
        if expected in reply.lower():
            return reply
        assert collector.poll() is None, reply[-3000:]
        time.sleep(.05)
    raise RuntimeError('Collector status acknowledgement unavailable: ' + repr(logfile.read_bytes()[since:][-3000:]))


def check_target(pid, args, expected_ticks):
    process = Path(f'/proc/{pid}')
    assert (process / 'cgroup').read_text().strip() == '0::' + args.expected_cgroup
    assert (process / 'stat').read_text().rsplit(')', 1)[1].split()[19] == expected_ticks
    assert (process / 'exe').resolve() == args.wasmtime.resolve()
    capabilities = {name:int(value,16) for name,value in re.findall(r'^(Cap(?:Inh|Prm|Eff|Amb)):\s*([0-9a-f]+)$',(process/'status').read_text(),re.M)}
    assert capabilities and all(value==0 for value in capabilities.values()),capabilities
    affinities = {p.name: sorted(os.sched_getaffinity(int(p.name))) for p in (process / 'task').iterdir()}
    assert affinities and all(value == [args.cpu] for value in affinities.values()), affinities
    return affinities


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'wasmtime', 'lldb', 'fixtures', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--case', required=True)
    parser.add_argument('--expected-cgroup', required=True)
    parser.add_argument('--cpu', type=int, required=True)
    parser.add_argument('--hybrid-pmu-companion', type=int)
    parser.add_argument('--driverless-hotspots', action='store_true')
    parser.add_argument('--drop-target-capabilities', action='store_true')
    parser.add_argument('--kind', choices=('hotspots', 'uarch-exploration'), default='hotspots')
    parser.add_argument('--pmu-mode', choices=('detailed', 'summary'), default='summary')
    parser.add_argument('--vtune', type=Path, default=Path('/opt/intel/oneapi/vtune/latest/bin64/vtune'))
    parser.add_argument('--timeout', type=int, default=240)
    parser.add_argument('--min-kernel-ms', type=int, default=1000)
    args = parser.parse_args()
    assert 1 <= args.timeout <= 600 and 100 <= args.min_kernel_ms <= 10000
    subprocess.run(['bash', str(args.source_root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    assert os.sched_getaffinity(0) == {args.cpu}
    assert not args.driverless_hotspots or args.kind=='hotspots'
    sampled_cpus=[args.cpu]
    if args.hybrid_pmu_companion is not None:
        def cpu_set(text):
            result=set()
            for part in text.strip().split(','):
                bounds=[int(v) for v in part.split('-')];result.update(range(bounds[0],bounds[-1]+1))
            return result
        core=cpu_set(Path('/sys/devices/cpu_core/cpus').read_text());atom=cpu_set(Path('/sys/devices/cpu_atom/cpus').read_text())
        assert (args.cpu in core and args.hybrid_pmu_companion in atom) or (args.cpu in atom and args.hybrid_pmu_companion in core)
        allowed=cpu_set(Path('/sys/fs/cgroup'+args.expected_cgroup+'/cpuset.cpus.effective').read_text())
        sampled_cpus=sorted([args.cpu,args.hybrid_pmu_companion]);assert set(sampled_cpus)<=allowed
    cg = Path('/sys/fs/cgroup' + args.expected_cgroup)
    assert Path('/proc/self/cgroup').read_text().strip() == '0::' + args.expected_cgroup
    assert (cg / 'memory.max').read_text().strip() == str(64 << 30)
    assert (cg / 'memory.swap.max').read_text().strip() == '0'
    assert resource.getrlimit(resource.RLIMIT_NOFILE)[0] >= 65536
    rows = json.loads((args.fixtures / 'manifest.json').read_text())
    selected = [row for row in rows if row['name'] == args.case]
    assert len(selected) == 1
    row = selected[0]
    assert row['validation'] == 'passed' and row['profile_gates'] == {'before': 'start', 'after': 'profile_end'}
    wasm = (args.fixtures / (args.case + '.wasm')).resolve(strict=True)
    assert digest(wasm) == row['wasm_sha256']
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    os.environ.update(TERM='dumb', NO_COLOR='1', DEBUGINFOD_URLS='')
    immutable_paths = [args.wasmtime, args.lldb, args.vtune, wasm, args.fixtures / 'manifest.json',
                       Path(__file__), Path(__file__).with_name('debug_sampling_safety.py'),
                       args.source_root / 'test/0017.runtime/run_debug_run_cli.py',
                       args.source_root / 'test/0017.runtime/run_debug_wasmtime_comparison.py']
    before = {str(path.resolve()): digest(path) for path in immutable_paths}
    data = {'passed': False, 'hardware_capture_passed': False, 'jit_symbol_attribution': False,
            'case': row, 'kind': args.kind, 'pmu_mode': args.pmu_mode if args.kind == 'uarch-exploration' else None,
            'cgroup': args.expected_cgroup, 'boot': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
            'immutable_before': before, 'minimum_kernel_ms': args.min_kernel_ms,
            'scope': 'start breakpoint before kernel clock through post-result profile_end breakpoint; includes output, excludes compilation/startup, guest exit and runtime teardown',
            'notes': ['LLDB uses the Wasm plugin; GNU GDB/native DWARF is a different baseline.',
                      'Wasmtime private StoreCode JIT symbol attribution must be checked in actual sampled rows, not inferred from --profile=vtune.',
                      'Profiled elapsed time cannot substitute for unprofiled throughput.']}
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    target = [str(args.wasmtime.resolve()), 'run', '-C', 'cache=n', '-O', 'opt-level=2',
              '-W', 'memory64=y', '--profile=vtune', '-g', f'127.0.0.1:{port}']
    if row['name'] == 'deterministic_profile':
        target += ['-W', 'nan-canonicalization=y', '-W', 'relaxed-simd-deterministic=y']
    target += [str(wasm)]
    data['target_argv'] = target
    data['sampled_cpus']=sampled_cpus
    data['collector_capabilities']={name:int(value,16) for name,value in re.findall(r'^(Cap(?:Inh|Prm|Eff|Amb)):\s*([0-9a-f]+)$',Path('/proc/self/status').read_text(),re.M)}
    launcher = args.out / 'launch.py'
    launcher.write_text(LAUNCHER)
    config = args.out / 'launch.json'
    config.write_text(json.dumps({'argv': target, 'cpu': args.cpu, 'cgroup': '0::' + args.expected_cgroup,
                                 'drop_capabilities':args.drop_target_capabilities, 'server_log': str(args.out / 'server.bin'), 'pid_receipt': str(args.out / 'pid.json')}))
    collect = [str(args.vtune.resolve()), *(['-collect-with','runsa'] if args.driverless_hotspots else ['-collect',args.kind]), '-result-dir', str(args.out / 'result'),
               '-start-paused', '-target-duration-type', 'veryshort']
    options=sampling_options(args.kind,args.pmu_mode,args.cpu);options[5]=','.join(str(cpu) for cpu in sampled_cpus)
    if args.driverless_hotspots:
        options=options[:6]+['-knob','enable-stack-collection=false','-knob','enable-driverless-collection=true',
            '-knob','event-config=CPU_CLK_UNHALTED.REF_TSC:sa=30000000,INST_RETIRED.ANY:sa=30000000']
    collect += options
    data['hardware_backend']='perf driverless custom clock/retired sampling' if args.driverless_hotspots else 'VTune preset'
    collect += ['--', str(Path(sys.executable).resolve()), str(launcher), str(config)]
    data['collect_argv'] = collect
    collector = console = target_owner = safety = None
    durable_write(args.out / 'receipt.json', json.dumps(data, indent=2))
    try:
        safety = SamplingSession(args.out,sampled_cpus=sampled_cpus)
        with (args.out / 'collect.log').open('xb') as log:
            collector = subprocess.Popen(collect, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
            deadline = time.monotonic() + args.timeout
            while True:
                safety.check()
                server_log = args.out / 'server.bin'
                if server_log.exists() and b'Debugger listening' in server_log.read_bytes(): break
                assert collector.poll() is None, (args.out / 'collect.log').read_bytes()[-3000:]
                assert time.monotonic() < deadline, 'Wasmtime listening deadline'
                time.sleep(.05)
            pid_receipt = json.loads((args.out / 'pid.json').read_text())
            pid = pid_receipt['pid']
            target_owner = os.pidfd_open(pid)
            data.update(target_pid=pid, target_task_affinities=check_target(pid, args, pid_receipt['start_ticks']))
            data['target_capabilities']={name:int(value,16) for name,value in re.findall(r'^(Cap(?:Inh|Prm|Eff|Amb)):\s*([0-9a-f]+)$',Path(f'/proc/{pid}/status').read_text(),re.M)}
            status_argv = [str(args.vtune), '-command', 'status', '-r', str(args.out / 'result')]
            status_offset = (args.out / 'collect.log').stat().st_size
            status = saved_run(status_argv, args.out / 'status.log')
            data['status_argv'] = status_argv
            status_ack = status if b'paused' in status.lower() else collection_status(collector, args.out / 'collect.log', status_offset, safety, b'collection paused')
            durable_write(args.out / 'status-ack.log', status_ack)
            data['paused_before_attach_verified'] = True
            packets = args.out / 'packets.log'
            console = Console([str(args.lldb.resolve()), '--no-lldbinit'])
            console.wait(safety.predicate(lambda output: re.search(rb'\(lldb\) $', output)), 0, args.timeout)
            lldb_command(console, 'settings set auto-confirm true', args.timeout, safety)
            lldb_command(console, 'log enable gdb-remote packets -f ' + json.dumps(str(packets)), args.timeout, safety)
            connected = lldb_command(console, f'process connect --plugin wasm connect://127.0.0.1:{port}', args.timeout, safety)
            assert b'Process 1 stopped' in connected, connected[-3000:]
            entry = lldb_command(console, 'breakpoint set -n start', args.timeout, safety)
            address = re.search(rb'Breakpoint 1: .*address = (0x[0-9a-fA-F]+)', entry)
            assert address and re.search(rb'where = .*`start(?:,|\s)', entry), entry[-3000:]
            end = lldb_command(console, 'breakpoint set -n profile_end', args.timeout, safety)
            assert re.search(rb'Breakpoint 2: .*where = .*`profile_end', end), end[-3000:]
            stopped = lldb_command(console, 'continue', args.timeout, safety)
            assert b'stop reason = breakpoint 1.1' in stopped, stopped[-3000:]
            pc = lldb_command(console, 'register read pc', args.timeout, safety)
            actual_pc = re.search(rb'pc = (0x[0-9a-fA-F]+)', pc)
            assert actual_pc and int(actual_pc[1], 16) == int(address[1], 16), pc[-3000:]
            lldb_command(console, 'thread backtrace', args.timeout, safety)
            lldb_command(console, 'breakpoint delete --force 1', args.timeout, safety)
            assert b'qWasmCallStack' in packets.read_bytes(), 'must exercise Wasm call stack plugin'
            data['pre_clock_wasm_pc'] = int(address[1], 16)
            resume_argv = [str(args.vtune), '-command', 'resume', '-r', str(args.out / 'result')]
            saved_run(resume_argv, args.out / 'resume.log')
            data['resume_argv'] = resume_argv
            stopped = lldb_command(console, 'continue', args.timeout, safety)
            assert b'stop reason = breakpoint 2.1' in stopped and b'`profile_end' in stopped, stopped[-3000:]
            output = server_log.read_bytes()
            times, sums = re.findall(rb'Kernel ns: (\d+)', output), re.findall(rb'Checksum: (\d+)', output)
            assert len(times) == len(sums) == 1 and int(sums[0]) == row['expected_checksum'], output[-3000:]
            data.update(kernel_ms=int(times[0]) / 1e6, checksum=int(sums[0]),
                        post_result_task_affinities=check_target(pid, args, pid_receipt['start_ticks']))
            # In Launch Application mode, stop may terminate the application.
            # Pause while the guest is stopped instead, retire it normally,
            # and let collection finalize with teardown still paused.
            pause_argv = [str(args.vtune), '-command', 'pause', '-r', str(args.out / 'result')]
            saved_run(pause_argv, args.out / 'pause.log')
            data['pause_argv'] = pause_argv
            status_offset = (args.out / 'collect.log').stat().st_size
            paused = saved_run(status_argv, args.out / 'post-status.log')
            status_ack = paused if b'paused' in paused.lower() else collection_status(collector, args.out / 'collect.log', status_offset, safety, b'collection paused')
            durable_write(args.out / 'post-status-ack.log', status_ack)
            data['paused_before_retirement_verified'] = True
            lldb_command(console, 'breakpoint delete --force 2', args.timeout, safety)
            exited = lldb_command(console, 'continue', args.timeout, safety)
            assert b'exited with status = 0' in exited, exited[-3000:]
            console.finish()
            data['collect_exit'] = collector.wait(timeout=180)
            assert data['collect_exit'] == 0, (args.out / 'collect.log').read_bytes()[-3000:]
        for report in ('summary', 'hotspots'):
            argv = [str(args.vtune), '-report', report, '-r', str(args.out / 'result'), '-format', 'csv']
            if report == 'hotspots': argv += ['-group-by', 'function', '-limit', '100']
            saved_run(argv, args.out / (report + '.csv'))
        data['metrics'] = [{'level': int(cells[0]), 'metric': cells[1], 'value': cells[2]}
                           for cells in csv.reader(io.StringIO((args.out / 'summary.csv').read_text()), delimiter='\t')
                           if len(cells) >= 3 and cells[0].isdigit()]
        assert data['kernel_ms'] >= args.min_kernel_ms, 'Increase --kernel-repeats for useful hardware sampling'
        if args.kind == 'uarch-exploration':
            retired = [r['value'] for r in data['metrics'] if r['metric'].strip() == 'Instructions Retired']
            assert len(retired) == 1 and float(retired[0].replace(',', '')) > 0
        if args.driverless_hotspots:
            summary = (args.out / 'summary.csv').read_text()
            header = re.search(r'Hardware Event Type\tCore Type\t[^\r\n]+', summary)
            assert header, 'Actual hardware event/sample count table required'
            events = csv.reader(io.StringIO(summary[header.start():]), delimiter='\t')
            fields = next(events)
            data['hardware_event_rows'] = [dict(zip(fields, cells)) for cells in events if len(cells) == len(fields)]
            for event in ['INST_RETIRED.ANY', 'CPU_CLK_UNHALTED.REF_TSC']:
                assert sum(int(row['Hardware Event Sample Count:Self']) for row in data['hardware_event_rows']
                           if row['Hardware Event Type'] == event) >= 10, 'At least 10 actual samples per event required'
        data['immutable_after'] = {path: digest(Path(path)) for path in before}
        assert data['immutable_after'] == before
        safety.check(force=True)
        data['hardware_capture_passed'] = True
        # EngineCode and the private executable StoreCode can have different
        # addresses. A load notification for an unused original is not proof
        # that the actually executed kernel has a name in hardware samples.
        # VTune writes progress on stderr using carriage returns. Since the
        # raw receipt intentionally retains both streams, the actual TSV
        # header can follow a progress message on the SAME physical line.
        # Locate the header without altering the retained report bytes, and
        # reject footer/progress records whose column count is different.
        report = (args.out / 'hotspots.csv').read_text()
        header = re.search(r'Function\t[^\r\n]+', report)
        table = []
        if header:
            records = csv.reader(io.StringIO(report[header.start():]), delimiter='\t')
            fields = next(records)
            assert 'Function' in fields, fields
            if args.kind == 'hotspots':
                assert ('Hardware Event Count:CPU_CLK_UNHALTED.REF_TSC' if args.driverless_hotspots else 'CPU Time') in fields,fields
            table = [dict(zip(fields, cells)) for cells in records if len(cells) == len(fields)]
        else:
            assert args.kind != 'hotspots', 'VTune function report header unavailable'
            data['notes'].append('This PMU capture has no function sampling table; only its whole-window counters qualify.')
        data['sampled_kernel_rows'] = [r for r in table if re.search(r'(?:^|\W)kernel(?:\W|$)', r.get('Function', ''))
                                      and any(float(v.replace(',', '')) > 0 for k, v in r.items()
                                              if k and ('CPU Time' in k or (args.driverless_hotspots and k=='Hardware Event Count:CPU_CLK_UNHALTED.REF_TSC')) and v and re.fullmatch(r'[0-9.,]+', v))]
        data['jit_symbol_attribution'] = bool(data['sampled_kernel_rows'])
        if args.kind == 'hotspots':
            assert data['jit_symbol_attribution'], 'JIT kernel symbol attribution unavailable; retain raw samples, do not claim named Wasm hotspots'
        data['passed'] = True
    except Exception as error:
        data['error'] = repr(error)
        raise
    finally:
        # Only handles created for this invocation can be stopped. A stale
        # numeric PID or a process merely named wasmtime grants no ownership.
        if target_owner is not None:
            try: signal.pidfd_send_signal(target_owner, signal.SIGKILL)
            except ProcessLookupError: pass
            finally: os.close(target_owner)
        if collector is not None and collector.poll() is None:
            collector.terminate()
            try: collector.wait(timeout=15)
            except subprocess.TimeoutExpired: collector.kill(); collector.wait(timeout=5)
        if console is not None:
            durable_write(args.out / 'transcript.bin', console.log)
            console.close()
        data['artifacts'] = {path.name: digest(path) for path in args.out.iterdir()
                             if path.is_file() and path.name != 'receipt.json'}
        if safety is not None:
            data['kernel_alerts'] = safety.events
            safety.close()
        sync_artifacts(args.out)
        durable_write(args.out / 'receipt.json', json.dumps(data, indent=2) + '\n')
    print(json.dumps({key: data[key] for key in ('passed', 'hardware_capture_passed', 'jit_symbol_attribution', 'kernel_ms')}), flush=True)


if __name__ == '__main__': main()

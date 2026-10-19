#!/usr/bin/env python3
"""Serial VTune hardware Hotspots and microarchitecture windows after startw.

The window includes language runtime initialization, guest work, output and
exit. Cold JIT and debugger teardown are excluded. This is not a kernel timer.
"""
import argparse
import csv
import fcntl
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

from build_products import admission, digest
from run_matrix import argv_for, output_matches, send, compile_affinity, target_affinity, checked_wait, process_memory
from bench_engines import (BENCHMARK_LOCK, select_cases, stable_checks, kernel_seconds,
                           validate_kernel_seconds, process_identity, check_admission)
from owned_process import remember_process, terminate_group, wait_process, owned_process_record


def cpu_set(text):
    result = set()
    for part in text.strip().split(','):
        bounds = [int(value) for value in part.split('-')]
        result.update(range(bounds[0], bounds[-1] + 1))
    return result


def target_record(pid, cpu):
    record = process_identity(pid)
    membership = Path('/proc/self/cgroup').read_text().strip()
    assert record['cgroup'] == membership and record['tasks'], 'target cgroup/tasks unavailable'
    for tid, row in record['tasks'].items():
        assert row['affinity'] == [cpu] and row['cgroup'] == membership, ('target TID placement', tid, row)
        status = Path(f'/proc/{pid}/task/{tid}/status').read_text()
        row['capabilities'] = {name: int(value, 16) for name, value in
                               re.findall(r'^(Cap(?:Inh|Prm|Eff|Bnd|Amb)):\s*([0-9a-f]+)$', status, re.M)}
        assert len(row['capabilities']) == 5 and not any(row['capabilities'].values()), ('target capabilities retained', tid, row)
        assert re.search(r'^NoNewPrivs:\s*1$', status, re.M), 'target no-new-privileges missing'
    return record


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--arena', type=Path, required=True)
    p.add_argument('--manifest', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--repository', required=True, choices=['uwvm2', 'uwvm2-stable', 'uwvm2-ros'])
    p.add_argument('--case', required=True)
    p.add_argument('--kind', required=True, choices=['hotspots', 'uarch-summary', 'uarch-detailed'])
    p.add_argument('--allow-detailed', action='store_true', help='explicit opt-in after this machine previously reported detailed-mode perf throttling')
    p.add_argument('--sampling-interval-ms', type=int, choices=[10, 50, 100], default=10)
    p.add_argument('--cpu', type=int, default=21)
    p.add_argument('--companion', type=int, default=9)
    p.add_argument('--vtune', type=Path, default=Path('/opt/intel/oneapi/vtune/latest/bin64/vtune'))
    a = p.parse_args()
    assert a.kind != 'uarch-detailed' or a.allow_detailed, 'detailed collection requires explicit --allow-detailed'
    before = admission()
    assert {a.cpu, a.companion} <= os.sched_getaffinity(0)
    os.sched_setaffinity(0, {a.cpu})
    lock = os.open(BENCHMARK_LOCK, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    runtime = Path(__file__).with_name('runtime')
    if not runtime.is_dir():
        runtime = a.arena / 'uwvm2-ros/test/0017.runtime'
    sys.path.insert(0, str(runtime))
    from debug_sampling_safety import SamplingSession, durable_write, sync_artifacts
    from run_debug_run_cli import Console
    case, = select_cases(json.loads(a.manifest.read_text()), [a.case])
    core = cpu_set(Path('/sys/devices/cpu_core/cpus').read_text())
    atom = cpu_set(Path('/sys/devices/cpu_atom/cpus').read_text())
    assert ((a.cpu in core and a.companion in atom) or (a.cpu in atom and a.companion in core)), 'target and companion must belong to opposite hybrid PMUs'
    binary = a.arena / 'products' / a.repository / 'uwvm'
    identity = json.loads((binary.parent / 'identity.json').read_text())
    assert identity['binary_sha256'] == digest(binary)
    a.out.mkdir(parents=True, exist_ok=False)
    helper = runtime / 'debug_sampling_safety.py'
    setpriv = Path('/usr/bin/setpriv')
    drop = [str(setpriv), '--bounding-set=-all', '--inh-caps=-all', '--ambient-caps=-all', '--no-new-privs']
    data = {'passed': False, 'kind': a.kind, 'case': a.case, 'repository': a.repository,
            'admission_before': before, 'binary_sha256': digest(binary), 'wasm_sha256': digest(case['wasm']),
            'manifest_sha256': digest(a.manifest), 'sampler_sha256': digest(__file__), 'sampling_helper_sha256': digest(helper),
            'benchmark_helpers_sha256': digest(Path(__file__).with_name('bench_engines.py')),
            'matrix_helpers_sha256': digest(Path(__file__).with_name('run_matrix.py')),
            'owned_process_helper_sha256': digest(Path(__file__).with_name('owned_process.py')),
            'setpriv_sha256': digest(setpriv), 'vtune_sha256': digest(a.vtune),
            'boot': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
            'hybrid_pmu': {'cpu_core': sorted(core), 'cpu_atom': sorted(atom), 'target': a.cpu, 'companion': a.companion},
            'collector_capabilities': {name: int(value, 16) for name, value in re.findall(r'^(Cap(?:Inh|Prm|Eff|Bnd|Amb)):\s*([0-9a-f]+)$', Path('/proc/self/status').read_text(), re.M)},
            'sampling_interval_ms': a.sampling_interval_ms,
            'custom_sampling_period_base': 30000000 if a.kind == 'hotspots' else None,
            'custom_sampling_period': 30000000 * a.sampling_interval_ms if a.kind == 'hotspots' else None,
            'custom_sampling_period_note': ('runsa scales event-config sa base by sampling-interval; raw configuration retained'
                                            if a.kind == 'hotspots' else
                                            'VTune microarchitecture preset selects event periods; custom Hotspots period is not applicable; raw configuration retained'),
            'kernel_clock': case.get('kernel_clock', 'guest-reported'), 'kernel_time': case['kernel_time'],
            'kernel_scope': 'guest timer around workload; narrower than the hardware collection window',
            'scope': 'after Wasm entry safe point through guest exit; includes language runtime initialization and IO; excludes cold JIT and debugger teardown'}
    durable_write(a.out / 'receipt.json', json.dumps(data, indent=2))
    c = collector = safety = None
    owned_commands = []
    success = False

    def observe():
        for child in owned_commands:
            remember_process(child)
        if safety is not None:
            safety.check()

    def owned_command(command, stream):
        child = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        owned_commands.append(child)
        remember_process(child, force=True)
        return child

    try:
        safety = SamplingSession(a.out, sampled_cpus=[a.cpu, a.companion], sampling_interval_ms=a.sampling_interval_ms)
        argv = [*drop, *argv_for(binary, a.repository, case, True)]
        data['target_argv'] = argv
        os.sched_setaffinity(0, compile_affinity())
        try:
            c = Console(argv)
        finally:
            os.sched_setaffinity(0, {a.cpu})
        checked_wait(c, lambda b: b'(uwvm-debug) ' in b, 0, case.get('initial_prompt_timeout', 600))
        data['cold_compile_process_memory'] = process_memory(c.pid)
        entry = send(c, 'startw', case.get('cold_command_timeout', 600))
        target_affinity(c.pid, {a.cpu})
        assert b'byte-offset=0 ' in entry and b'stopped:' in entry, entry[-3000:]
        data.update(target_pid=c.pid, target_at_entry=target_record(c.pid, a.cpu),
                    start_reply=entry.decode(errors='replace'))
        options = ['-duration', '30', '-knob', 'sampling-interval=' + str(a.sampling_interval_ms), '-cpu-mask', f'{a.companion},{a.cpu}',
                   '-knob', 'enable-stack-collection=false']
        if a.kind == 'hotspots':
            collect = ['-collect-with', 'runsa']
            options += ['-knob', 'enable-driverless-collection=true', '-knob',
                        'event-config=CPU_CLK_UNHALTED.REF_TSC:sa=30000000,INST_RETIRED.ANY:sa=30000000']
            data['backend'] = 'driverless hardware clock/instructions sampling, no stacks'
        else:
            collect = ['-collect', 'uarch-exploration']
            options += ['-knob', 'pmu-collection-mode=' + ('summary' if a.kind == 'uarch-summary' else 'detailed')]
            data['backend'] = 'VTune microarchitecture hardware preset'
        command = [str(a.vtune), *collect, '-result-dir', str(a.out / 'result'), '-target-pid', str(c.pid), *options]
        data['collect_argv'] = command
        with (a.out / 'collect.log').open('wb') as log:
            collector = owned_command(command, log)
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline and collector.poll() is None:
                observe()
                text = (a.out / 'collect.log').read_text(errors='replace')
                if 'Collection started' in text or 'Collection resumed' in text:
                    break
                time.sleep(.1)
            else:
                raise RuntimeError('hardware collection failed to start: ' + (a.out / 'collect.log').read_text(errors='replace')[-4000:])
            start = time.monotonic()
            at = len(c.log)
            os.write(c.fd, b'continue\n')
            def exited(output):
                observe()
                return b'Program exited' in output and re.search(rb'(?:^|\n)\(uwvm-debug\) $', output)
            reply = checked_wait(c, exited, at, 28)
            data['guest_window_seconds'] = time.monotonic() - start
            data['target_after_guest_exit'] = target_record(c.pid, a.cpu)
            assert b'Program exited normally.' in reply and output_matches(reply, stable_checks(case)), reply[-4000:]
            data['kernel_seconds'] = kernel_seconds(reply.decode(errors='replace'), case)
            validate_kernel_seconds(data['kernel_seconds'], data['guest_window_seconds'])
            assert collector.poll() is None, 'collector ended before guest exit'
            with (a.out / 'stop.log').open('wb') as stop_log:
                stop = owned_command([str(a.vtune), '-command', 'stop', '-r', str(a.out / 'result')], stop_log)
                stop_exit = wait_process(stop, 120, observe)
            assert stop_exit == 0, (a.out / 'stop.log').read_bytes()[-4000:]
            data['collector_exit'] = wait_process(collector, 180, observe)
            assert data['collector_exit'] == 0
        c.finish()
        for report in ['summary', 'hotspots']:
            command = [str(a.vtune), '-report', report, '-r', str(a.out / 'result'), '-format', 'csv']
            if report == 'hotspots':
                command += ['-group-by', 'function', '-limit', '40']
            with (a.out / (report + '.csv')).open('wb') as report_log:
                q = owned_command(command, report_log)
                report_exit = wait_process(q, 120, observe)
            assert report_exit == 0, (a.out / (report + '.csv')).read_bytes()[-4000:]
        summary = (a.out / 'summary.csv').read_text(errors='replace')
        if a.kind == 'hotspots':
            match = re.search(r'Hardware Event Type\tCore Type\t[^\r\n]+', summary)
            assert match, summary[-4000:]
            reader = csv.DictReader(io.StringIO(summary[match.start():]), delimiter='\t')
            rows = list(reader)
            data['hardware_events'] = rows
            # The opposite hybrid PMU CPU is required by VTune's setup, but
            # cannot qualify the target's samples. All guest TIDs run on a.cpu.
            target_is_atom = a.cpu in atom
            data['target_pmu'] = 'cpu_atom' if target_is_atom else 'cpu_core'
            target_rows = [row for row in rows if bool(re.search(r'atom|efficien|e-core', row.get('Core Type', ''), re.I)) == target_is_atom]
            assert target_rows and all(row.get('Core Type') for row in target_rows), 'target PMU report rows unavailable'
            data['target_hardware_events'] = target_rows
            for event in ['INST_RETIRED.ANY', 'CPU_CLK_UNHALTED.REF_TSC']:
                count = sum(int(row['Hardware Event Sample Count:Self']) for row in target_rows if row.get('Hardware Event Type') == event)
                assert count >= 10, f'only {count} actual samples for {event}; increase guest work'
            text = (a.out / 'hotspots.csv').read_text()
            match = re.search(r'Function\t[^\r\n]+', text)
            assert match, 'hardware function report is required'
            reader = csv.DictReader(io.StringIO(text[match.start():]), delimiter='\t')
            key = 'Hardware Event Count:CPU_CLK_UNHALTED.REF_TSC'
            assert key in reader.fieldnames
            data['sampled_function_rows'] = [row for row in reader if re.fullmatch(r'[0-9.,]+', row.get(key) or '')
                                             and float(row[key].replace(',', '')) > 0]
            assert data['sampled_function_rows'], 'nonzero hardware function samples are required'
        else:
            rows = list(csv.reader(io.StringIO(summary), delimiter='\t'))
            metrics = [{'metric': cells[1].strip(), 'value': cells[2]} for cells in rows if len(cells) >= 3 and cells[0].isdigit()]
            data['metrics'] = metrics
            retired = [row['value'] for row in metrics if row['metric'] == 'Instructions Retired']
            assert len(retired) == 1 and float(retired[0].replace(',', '')) > 0
        assert 1 <= data['guest_window_seconds'] < 28, 'guest window outside 1..28 seconds'
        assert digest(binary) == data['binary_sha256'] and digest(case['wasm']) == data['wasm_sha256']
        assert digest(a.manifest) == data['manifest_sha256'] and digest(helper) == data['sampling_helper_sha256']
        assert digest(Path(__file__).with_name('bench_engines.py')) == data['benchmark_helpers_sha256']
        assert digest(Path(__file__).with_name('run_matrix.py')) == data['matrix_helpers_sha256']
        assert digest(Path(__file__).with_name('owned_process.py')) == data['owned_process_helper_sha256']
        assert digest(a.vtune) == data['vtune_sha256'] and digest(setpriv) == data['setpriv_sha256']
        safety.check(force=True)
        success = True
    except BaseException as error:
        data['error'] = repr(error)
        raise
    finally:
        try:
            cleanup_errors = []
            data['kernel_alerts'] = safety.events if safety is not None else []
            for action in [*[lambda child=child: terminate_group(child) for child in reversed(owned_commands)],
                           lambda: (a.out / 'console.log').write_bytes(c.log) if c is not None else None,
                           lambda: c.close() if c is not None else None,
                           lambda: safety.close() if safety is not None else None]:
                try:
                    action()
                except Exception as error:
                    cleanup_errors.append(repr(error))
            data['owned_processes'] = [owned_process_record(child) for child in owned_commands]
            try:
                data['admission_after'] = admission()
                check_admission(before, data['admission_after'])
                assert Path('/proc/sys/kernel/random/boot_id').read_text().strip() == data['boot']
            except Exception as error:
                cleanup_errors.append(repr(error))
            if cleanup_errors:
                data['cleanup_errors'] = cleanup_errors
            data['passed'] = False
            durable_write(a.out / 'receipt.json', json.dumps(data, indent=2, allow_nan=False))
            # Persist every raw dataset, transcript and safety record before
            # publishing the final success receipt. A crash leaves passed=false.
            try:
                sync_artifacts(a.out)
                data['artifacts_synced_before_receipt'] = True
                data['passed'] = success and not cleanup_errors
            except Exception as error:
                data['sync_error'] = repr(error)
                raise
            finally:
                durable_write(a.out / 'receipt.json', json.dumps(data, indent=2, allow_nan=False))
            if success and not data['passed']:
                raise RuntimeError('collection evidence cleanup/admission failed: ' + repr(cleanup_errors))
        finally:
            os.close(lock)
    print(json.dumps({key: data[key] for key in ['passed', 'kind', 'case', 'repository', 'guest_window_seconds']}), flush=True)


if __name__ == '__main__':
    main()

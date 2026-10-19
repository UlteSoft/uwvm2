#!/usr/bin/env python3
"""Serialize gated VTune hardware Hotspots and detailed microarchitecture runs.

Uses the repository's existing kernel-journal sampling guard and PID/TID/CPU
sample qualifier. No PMU policy, driver, sysctl or cache state is changed.
"""
import argparse
from decimal import Decimal
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time


def actual_hotspots_period(options, digest):
    """Check the actual collector file without importing unrelated HLL runners."""
    text = Path(options).read_text()
    configurations = re.findall(r'^--event-config=(.+)$', text, re.M)
    multipliers = re.findall(r'^--sample-after-multiplier=([^\n]+)$', text, re.M)
    if len(configurations) != 1 or len(multipliers) != 1:
        raise RuntimeError('one actual event configuration/multiplier required')
    events = dict(re.findall(r'([A-Z0-9_.]+):sa=([0-9]+)', configurations[0]))
    expected = {'CPU_CLK_UNHALTED.REF_TSC': '20000000', 'INST_RETIRED.ANY': '20000000'}
    if events != expected or configurations[0].count(',') != 1:
        raise RuntimeError('actual Hotspots event bases differ')
    multiplier = Decimal(multipliers[0])
    if not multiplier.is_finite() or multiplier != 10:
        raise RuntimeError('actual dimensionless SAV multiplier differs from requested knob10')
    return {'event_bases': {name: int(value) for name, value in events.items()},
            'sample_after_multiplier': str(multiplier), 'actual_event_period': 200000000,
            'actual_event_units': {'CPU_CLK_UNHALTED.REF_TSC': 'reference clock ticks, not core cycles',
                                   'INST_RETIRED.ANY': 'retired instructions'},
            'interval_limit': 'Requested sampling-interval knob10 is an SAV multiplier; no measured10ms wall-clock period or IPC is asserted',
            'runsa_options_sha256': digest(options)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cpu', type=int, required=True)
    parser.add_argument('--companion', type=int, required=True)
    parser.add_argument('--iterations', type=int, required=True)
    parser.add_argument('--maximum-memory-bytes', type=int, required=True,
                        help='allocated total cgroup ceiling; reactive stop 128MiB early')
    parser.add_argument('--maximum-seconds', type=int, default=600)
    parser.add_argument('--kind', choices=('hotspots', 'uarch-detailed'), required=True)
    parser.add_argument('--vtune', type=Path, default=Path('/opt/intel/oneapi/vtune/2026.4/bin64/vtune'))
    args = parser.parse_args()
    if not 1 <= args.iterations <= 100000000:
        parser.error('bounded positive iterations required')
    if not 128 << 20 < args.maximum_memory_bytes <= 64 << 30 or not 1 <= args.maximum_seconds <= 3600:
        parser.error('finite memory ceiling within 64GiB and finite <=1h allocation required')
    spec = importlib.util.spec_from_file_location('pretty_matrix_admission', Path(__file__).with_name('run_pretty_printer_matrix.py'))
    module = importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    before = module.admit()
    if not {args.cpu, args.companion} <= os.sched_getaffinity(0):
        raise RuntimeError('selected PMU CPUs are outside admitted affinity')
    os.sched_setaffinity(0, {args.cpu})
    sys.path.insert(0, str(args.source_root / 'test/0017.runtime'))
    sys.path.insert(0, str(args.source_root / 'test/0050.high_level_languages'))
    from debug_sampling_safety import SamplingSession, durable_write, sync_artifacts
    from owned_process import remember_process, wait_process, terminate_group, owned_process_record
    from vtune_reports import hardware_reports
    def cpus(path):
        out = set()
        for part in path.read_text().strip().split(','):
            limits = list(map(int, part.split('-')))
            out.update(range(limits[0], limits[-1] + 1))
        return out
    atom = cpus(Path('/sys/devices/cpu_atom/cpus'))
    core = cpus(Path('/sys/devices/cpu_core/cpus'))
    if not ((args.cpu in atom and args.companion in core) or (args.cpu in core and args.companion in atom)):
        raise RuntimeError('one target PMU and opposite hybrid PMU companion required')
    args.out.mkdir(mode=0o700, parents=True, exist_ok=False)
    data = {'passed': False, 'scope': 'gated copied-DATA wasm32 vector16/copy8 plus production object tree; initialization and teardown excluded, small gate fringe included',
            'comparison_limit': 'Only the wasm32 vector16/copy8 steady loop is sampled; not all container profiles or real DWARF images. No end-to-end GDB/LLDB parity, core-cycle IPC, or actual10ms interval claim.',
            'admission': before, 'kind': args.kind, 'iterations': args.iterations,
            'hybrid_pmu': {'target': args.cpu, 'companion': args.companion},
            'cleanup_errors': [], 'performance_eligible': True, 'vtune_sha256': module.sha(args.vtune), 'binary_sha256': module.sha(args.binary),
            'maximum_memory_bytes': args.maximum_memory_bytes,
            'memory_stop_threshold_bytes': args.maximum_memory_bytes - module.MEMORY_RESERVE,
            'reactive_memory_reserve_bytes': module.MEMORY_RESERVE, 'delegated_memory_cap': False,
            'disk_reserve_bytes': module.DISK_RESERVE, 'observed_memory_peak_bytes': 0,
            'helper_sha256': {str(p): module.sha(p) for p in (Path(__file__), Path(module.__file__),
                args.source_root / 'test/0017.runtime/debug_sampling_safety.py',
                args.source_root / 'test/0050.high_level_languages/vtune_reports.py',
                args.source_root / 'test/0050.high_level_languages/owned_process.py')}}
    receipt = args.out / 'receipt.json'
    target = collector = safety = None
    commands = []
    streams = []
    began_all = time.monotonic()
    def publish():
        durable_write(receipt, json.dumps(data, indent=2) + '\n')
    def cleanup_error(label, issue):
        data['cleanup_errors'].append({'operation': label, 'error': type(issue).__name__ + ': ' + str(issue)})
        data['passed'] = False; data['performance_eligible'] = False
    def observe():
        remember_process(target);remember_process(collector)
        current = int(Path('/sys/fs/cgroup' + module.CGROUP + '/memory.current').read_text())
        data['observed_memory_peak_bytes'] = max(data['observed_memory_peak_bytes'], current)
        root_free, output_free = shutil.disk_usage('/').free, shutil.disk_usage(args.out).free
        data['last_disk_observation'] = {'rootfs_free_bytes': root_free, 'outputfs_free_bytes': output_free}
        if current >= data['memory_stop_threshold_bytes']:
            raise RuntimeError('total cgroup memory.current reached the reactive stop threshold 128MiB below the allocated ceiling')
        if min(root_free, output_free) < module.DISK_RESERVE:
            raise RuntimeError('root/output free disk below the 3GiB reserve')
        if time.monotonic() - began_all >= args.maximum_seconds:
            raise TimeoutError('allocated profiling window expired')
        if safety is not None:
            safety.check()
    def marker(text, timeout):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            observe()
            if text in (args.out / 'stderr.log').read_bytes():
                return
            if target.poll() is not None:
                raise RuntimeError('benchmark ended before gate: ' + text.decode())
            time.sleep(.05)
        raise TimeoutError('benchmark gate timeout: ' + text.decode())
    def owned_command(argv, output, error, timeout):
        with Path(output).open('wb') as out, Path(error).open('wb') as err:
            observe()
            child = subprocess.Popen(argv, stdout=out, stderr=err, start_new_session=True,
                                     env=dict(os.environ, LC_ALL='C', LANG='C'))
            commands.append(child);remember_process(child)
            try:
                return wait_process(child, timeout, observe=observe)
            finally:
                try:
                    terminate_group(child)
                except BaseException as issue:
                    cleanup_error('terminate report/stop command', issue)
                    raise
    publish()
    try:
        observe()
        safety = SamplingSession(args.out, sampled_cpus=[args.cpu, args.companion], sampling_interval_ms=10)
        for name in ('stdout', 'stderr'):
            streams.append((args.out / (name + '.log')).open('wb'))
        argv = [str(args.binary), '--iterations', str(args.iterations), '--gate']
        observe()
        target = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=streams[0], stderr=streams[1], start_new_session=True)
        remember_process(target);data['target_pid'] = target.pid
        marker(b'UWVM_PRETTY_READY\n', 30)
        options = ['-duration', '30', '-knob', 'sampling-interval=10', '-cpu-mask', f'{args.companion},{args.cpu}',
                   '-knob', 'enable-stack-collection=false']
        if args.kind == 'hotspots':
            collect = ['-collect-with', 'runsa']
            options += ['-knob', 'enable-driverless-collection=true', '-knob',
                        'event-config=CPU_CLK_UNHALTED.REF_TSC:sa=20000000,INST_RETIRED.ANY:sa=20000000']
        else:
            collect = ['-collect', 'uarch-exploration']
            options += ['-knob', 'pmu-collection-mode=detailed']
        command = [str(args.vtune), *collect, '-result-dir', str(args.out / 'result'), '-target-pid', str(target.pid), *options]
        data['collect_argv'] = command
        with (args.out / 'collect.log').open('wb') as log:
            observe()
            collector = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            remember_process(collector)
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline and collector.poll() is None:
                observe()
                text = (args.out / 'collect.log').read_text(errors='replace')
                if 'Collection started' in text or 'Collection resumed' in text:
                    break
                time.sleep(.1)
            else:
                raise RuntimeError('hardware collector did not start; see collect.log')
            began = time.monotonic();target.stdin.write(b'G');target.stdin.flush()
            marker(b'UWVM_PRETTY_DONE\n', 25)
            data['profile_window_seconds'] = time.monotonic() - began
            if collector.poll() is not None:
                raise RuntimeError('collector ended before DONE gate')
            code = owned_command([str(args.vtune), '-command', 'stop', '-r', str(args.out / 'result')],
                                 args.out / 'stop.log', args.out / 'stop.stderr.log', 30)
            if code:
                raise RuntimeError('VTune stop failed')
            data['collector_exit'] = wait_process(collector, 180, observe=observe)
            if data['collector_exit']:
                raise RuntimeError('VTune collector failed')
        target.stdin.write(b'D');target.stdin.flush()
        data['target_exit'] = wait_process(target, 30, observe=observe)
        if data['target_exit']:
            raise RuntimeError('benchmark failed')
        data['benchmark'] = json.loads((args.out / 'stdout.log').read_text())
        data['kernel_seconds'] = data['benchmark']['nanoseconds'] / 1e9
        if not 1 <= data['kernel_seconds'] <= 20:
            raise RuntimeError('actual benchmark duration outside 1..20 seconds; recalibrate iterations')
        if args.kind == 'hotspots':
            data['actual_hotspots_period'] = actual_hotspots_period(args.out / 'result/config/runsa.options', module.sha)
        publish()
        data.update(hardware_reports(args.vtune, args.out, args.kind, args.cpu in atom,
                                     owned_command, target_pid=target.pid, target_cpu=args.cpu))
        data['admission_after'] = module.admit()
        data['passed'] = True
    except BaseException as error:
        data['error'] = type(error).__name__ + ': ' + str(error)
        data['passed'] = False; data['performance_eligible'] = False
        raise
    finally:
        # One undrainable helper must not skip the other owned processes,
        # kernel tail check, stream close or final failure receipt.
        for index, child in enumerate((*commands, collector, target)):
            if child is not None:
                try:
                    terminate_group(child)
                except BaseException as issue:
                    cleanup_error('terminate owned process ' + str(index), issue)
        if safety is not None:
            try:
                # Polling check() is throttled. Check the final journal tail
                # after collection stops and before releasing the sampler lock.
                safety.check(force=True)
                data['final_kernel_check'] = {'passed': True, 'cursor': safety.cursor, 'boot': safety.boot}
            except BaseException as issue:
                data['final_kernel_check'] = {'passed': False, 'error': repr(issue)}
                cleanup_error('final kernel journal check', issue)
            finally:
                try:
                    safety.close()
                except BaseException as issue:
                    cleanup_error('close sampling safety lock', issue)
        for index, stream in enumerate(streams):
            try:
                stream.close()
            except BaseException as issue:
                cleanup_error('close stream ' + str(index), issue)
        try:
            data['process_ownership'] = {'target': owned_process_record(target), 'collector': owned_process_record(collector),
                                         'commands': [owned_process_record(child) for child in commands]}
        except BaseException as issue:
            cleanup_error('record process ownership', issue)
        data['seconds'] = time.monotonic() - began_all
        try:
            sync_artifacts(args.out)
        except BaseException as issue:
            cleanup_error('sync artifacts', issue)
        finally:
            publish()
    if not data['passed']:
        raise RuntimeError('profiling cleanup/kernel qualification failed; see receipt.json')
    print(json.dumps({'passed': data['passed'], 'kind': args.kind, 'target_samples': data['target_samples']}))


if __name__ == '__main__':
    main()

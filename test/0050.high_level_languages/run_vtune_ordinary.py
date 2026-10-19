#!/usr/bin/env python3
"""Ordinary hardware profiling between two standard WASI stdin gates.

Profile-only compiler-produced modules retain their own identities. Native
compilation, language initialization and teardown are outside the collection;
the window includes kernel clocks and the DONE marker's WASI IO. No Rdbg or
debug-control channel is enabled. Ungated artifacts supply engine benchmarks.
"""
import argparse
from decimal import Decimal
import fcntl
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

from build_products import admission, digest
from bench_engines import (BENCHMARK_LOCK, select_cases, stable_checks,
                           kernel_seconds, validate_kernel_seconds, check_admission)
from run_matrix import argv_for, output_matches, process_memory
from run_vtune import (cpu_set, target_record, terminate_group, remember_process,
                       wait_process, owned_process_record)
from vtune_reports import hardware_reports
from prepare_profile_libraries import profile_environment


HOTSPOTS_EVENT_PERIOD_BASE = 20000000


def actual_hotspots_period(options):
    """Qualify the actual runsa base and multiplier, rather than the CLI guess."""
    text = Path(options).read_text()
    configurations = re.findall(r'^--event-config=(.+)$', text, re.M)
    multipliers = re.findall(r'^--sample-after-multiplier=([^\n]+)$', text, re.M)
    assert len(configurations) == len(multipliers) == 1, 'one actual event configuration/multiplier required'
    events = dict(re.findall(r'([A-Z0-9_.]+):sa=([0-9]+)', configurations[0]))
    expected = {'CPU_CLK_UNHALTED.REF_TSC': str(HOTSPOTS_EVENT_PERIOD_BASE),
                'INST_RETIRED.ANY': str(HOTSPOTS_EVENT_PERIOD_BASE)}
    assert events == expected and configurations[0].count(',') == 1, 'actual Hotspots event base differs'
    multiplier = Decimal(multipliers[0])
    assert multiplier.is_finite() and multiplier == 10, 'actual sampling multiplier differs from requested 10 ms'
    period = Decimal(HOTSPOTS_EVENT_PERIOD_BASE) * multiplier
    assert period == 200000000
    return {'event_bases': {name: int(value) for name, value in events.items()},
            'sample_after_multiplier': str(multiplier), 'actual_event_period': int(period),
            'runsa_options_sha256': digest(options)}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['arena', 'manifest', 'out']:
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--repository', choices=['uwvm2', 'uwvm2-stable', 'uwvm2-ros'], required=True)
    p.add_argument('--products-root', type=Path,
                   help='explicit immutable UWVM product directory; defaults to arena/products')
    p.add_argument('--case', required=True)
    p.add_argument('--kind', choices=['hotspots', 'uarch-summary', 'uarch-detailed'], required=True)
    p.add_argument('--calibrate-only', action='store_true', help='validate ordinary gates/timer without collecting hardware events')
    p.add_argument('--setup-probe', action='store_true', help='exclude this collector connectivity probe from final performance results')
    p.add_argument('--capture-jit-metadata', action='store_true', help='read registered JIT ELF metadata at READY before sampling')
    p.add_argument('--profile-libraries', type=Path,
                   help='private library receipt directory for this immutable cohort')
    p.add_argument('--cpu', type=int, default=21)
    p.add_argument('--companion', type=int, default=9)
    p.add_argument('--vtune', type=Path, default=Path('/opt/intel/oneapi/vtune/latest/bin64/vtune'))
    a = p.parse_args()
    assert a.kind != 'uarch-summary' or a.setup_probe or a.calibrate_only, 'summary counting is a setup probe only; use uarch-detailed for final process samples'
    before = admission()
    assert {a.cpu, a.companion} <= os.sched_getaffinity(0)
    os.sched_setaffinity(0, {a.cpu})
    os.environ.update(TERM='dumb', NO_COLOR='1', LC_ALL='C', LANG='C')
    lock = os.open(BENCHMARK_LOCK, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    runtime = Path(__file__).with_name('runtime')
    if not runtime.is_dir():
        runtime = a.arena / 'uwvm2-ros/test/0017.runtime'
    sys.path.insert(0, str(runtime))
    from debug_sampling_safety import SamplingSession, durable_write, sync_artifacts
    case, = select_cases(json.loads(a.manifest.read_text()), [a.case])
    assert case['purpose'] == 'hardware-gated'
    assert case['gate_stream'] in ['stdout', 'stderr']
    assert case['gate_ready'] == 'UWVM_PROFILE_READY' and case['gate_done'] == 'UWVM_PROFILE_DONE'
    assert case['gate_release'] == 'G' and case['gate_finish'] == 'D'
    core = cpu_set(Path('/sys/devices/cpu_core/cpus').read_text())
    atom = cpu_set(Path('/sys/devices/cpu_atom/cpus').read_text())
    assert ((a.cpu in core and a.companion in atom) or (a.cpu in atom and a.companion in core))
    products_root = a.products_root if a.products_root is not None else a.arena / 'products'
    binary = products_root / a.repository / 'uwvm'
    identity = json.loads((binary.parent / 'identity.json').read_text())
    assert identity['binary_sha256'] == digest(binary)
    target_environment, library_proof = profile_environment(a.arena, a.profile_libraries)
    public_binary = Path(library_proof['binaries'][a.repository]['public'])
    public_wasm = Path(library_proof['wasms'][digest(case['wasm'])]['public'])
    assert digest(public_binary) == identity['binary_sha256']
    assert digest(public_wasm) == digest(case['wasm'])
    execution_case = dict(case, wasm=str(public_wasm))
    setpriv = Path('/usr/bin/setpriv')
    argv = [str(setpriv), '--bounding-set=-all', '--inh-caps=-all', '--ambient-caps=-all', '--no-new-privs',
            *argv_for(public_binary, a.repository, execution_case, False)]
    assert '-Rdbg' not in argv and '-Rct' in argv and argv[argv.index('-Rct') + 1] == '0'
    a.out.mkdir(parents=True, exist_ok=False)
    helpers = {str(path): digest(path) for path in [
        Path(__file__), Path(__file__).with_name('bench_engines.py'),
        Path(__file__).with_name('run_matrix.py'), Path(__file__).with_name('run_vtune.py'),
        Path(__file__).with_name('vtune_reports.py'),
        Path(__file__).with_name('owned_process.py'),
        Path(__file__).with_name('prepare_profile_libraries.py'),
        runtime / 'debug_sampling_safety.py']}
    if a.capture_jit_metadata:
        helpers[str(Path(__file__).with_name('capture_jit_symbols.py'))] = digest(Path(__file__).with_name('capture_jit_symbols.py'))
    data = {'passed': False, 'mode': 'ordinary-gated-calibration' if a.calibrate_only else 'ordinary-gated', 'kind': 'calibration' if a.calibrate_only else a.kind, 'case': a.case,
            'performance_eligible': not a.calibrate_only and not a.setup_probe,
            'measurement_context': 'collector setup probe; concurrent functional tests; excluded from performance results' if a.setup_probe else ('duration/protocol calibration only' if a.calibrate_only else 'isolated final hardware profile'),
            'repository': a.repository, 'product': identity, 'products_root': str(products_root), 'admission_before': before,
            'boot': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
            'binary_sha256': digest(binary), 'wasm_sha256': digest(case['wasm']),
            'manifest_sha256': digest(a.manifest), 'helper_sha256': helpers,
            'setpriv_sha256': digest(setpriv), 'vtune_sha256': digest(a.vtune), 'target_argv': argv,
            'hybrid_pmu': {'cpu_core': sorted(core), 'cpu_atom': sorted(atom), 'target': a.cpu, 'companion': a.companion},
            'kernel_clock': case['kernel_clock'], 'kernel_time': case['kernel_time'],
            'scope': 'ordinary kernel between WASI stdin gates; includes timer syscalls and DONE marker IO; excludes native compilation, language initialization, output after DONE and teardown',
            'artifact_scope': 'independent gated profiling artifact; not the ungated engine benchmark module',
            'calibration_only': a.calibrate_only, 'sampling_interval_ms': None if a.calibrate_only else 10,
            'custom_sampling_period_base': HOTSPOTS_EVENT_PERIOD_BASE if not a.calibrate_only and a.kind == 'hotspots' else None,
            'custom_sampling_period': HOTSPOTS_EVENT_PERIOD_BASE * 10 if not a.calibrate_only and a.kind == 'hotspots' else None,
            'custom_sampling_period_note': 'runsa event-config sa base times actual sample-after-multiplier; actual config verified after collection' if a.kind == 'hotspots' and not a.calibrate_only else None,
            'period_scope': 'no hardware collection' if a.calibrate_only else ('custom runsa events' if a.kind == 'hotspots' else 'VTune uarch ' + a.kind.removeprefix('uarch-') + ' preset; retain actual raw configuration'),
            'memory_observations': []}
    data['runtime_library_copies'] = library_proof
    durable_write(a.out / 'receipt.json', json.dumps(data, indent=2))
    target = collector = safety = None
    handles = []
    commands = []
    success, last_memory = False, 0
    start = time.monotonic()
    cgroup = Path('/sys/fs/cgroup') / before['membership'][3:].lstrip('/')

    def contents(stream):
        return (a.out / (stream + '.log')).read_bytes()

    def check_target():
        nonlocal last_memory
        remember_process(target)
        remember_process(collector)
        if safety is not None:
            safety.check()
        if time.monotonic() - last_memory >= .5:
            memory = process_memory(target.pid)
            anon = int(dict(line.split() for line in (cgroup / 'memory.stat').read_text().splitlines())['anon'])
            data['memory_observations'].append({'elapsed_seconds': time.monotonic() - start,
                                                **memory, 'cgroup_anon_bytes': anon})
            assert memory['VmRSS'] < 56 << 30 and anon < 60 << 30, 'ordinary target memory budget exceeded'
            assert (cgroup / 'memory.events').read_text() == before['memory_events'], 'cgroup memory pressure during profile'
            last_memory = time.monotonic()

    def wait_marker(marker, timeout):
        deadline = time.monotonic() + timeout
        pattern = rb'(?:^|\n)' + marker.encode() + rb'\r?(?:\n|$)'
        while time.monotonic() < deadline:
            matches = list(re.finditer(pattern, contents(case['gate_stream'])))
            if matches:
                assert len(matches) == 1, 'duplicate gate marker'
                check_target()
                return
            assert target.poll() is None, 'guest exited before gate: ' + repr(contents('stderr')[-4000:])
            check_target()
            time.sleep(.05)
        raise TimeoutError('guest gate timeout: ' + marker)

    def observe_commands():
        remember_process(target)
        remember_process(collector)
        if safety is not None:
            safety.check()

    def owned_command(command, out, err, timeout):
        with Path(out).open('wb') as output, Path(err).open('wb') as error:
            child = subprocess.Popen(command, stdout=output, stderr=error, start_new_session=True)
            commands.append(child)
            remember_process(child)
            return wait_process(child, timeout, observe=observe_commands)

    def run_kernel(timeout):
        kernel_start = time.monotonic()
        target.stdin.write(b'G')
        target.stdin.flush()
        wait_marker(case['gate_done'], timeout)
        data['guest_window_seconds'] = time.monotonic() - kernel_start
        data['target_at_done'] = target_record(target.pid, a.cpu)
        early_stdout = contents('stdout').splitlines()
        assert early_stdout == ([b'UWVM_PROFILE_READY', b'UWVM_PROFILE_DONE'] if case['gate_stream'] == 'stdout' else []), 'semantic output appeared before D'

    try:
        if not a.calibrate_only:
            safety = SamplingSession(a.out, sampled_cpus=[a.cpu, a.companion], sampling_interval_ms=10)
        for stream in ['stdout', 'stderr']:
            handles.append((a.out / (stream + '.log')).open('wb'))
        target = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=handles[0], stderr=handles[1], start_new_session=True,
                                  env=target_environment)
        remember_process(target)
        data['target_pid'] = target.pid
        wait_marker(case['gate_ready'], case.get('gate_ready_timeout', 900))
        data['cold_ready_seconds'] = time.monotonic() - start
        data['target_at_ready'] = target_record(target.pid, a.cpu)
        if case['gate_stream'] == 'stderr':
            assert not contents('stdout'), 'semantic output appeared before READY release'
        assert not re.search(rb'(?:^|\n)UWVM_PROFILE_DONE\r?(?:\n|$)', contents(case['gate_stream'])), 'DONE before release'
        if a.capture_jit_metadata:
            from capture_jit_symbols import capture
            data['jit_metadata'] = capture(target.pid, public_binary, public_wasm, a.out / 'jit-metadata',
                                           expected_binary_sha256=data['binary_sha256'],
                                           expected_wasm_sha256=data['wasm_sha256'])
            assert data['jit_metadata'].get('safety_checks_passed') and not data['jit_metadata'].get('fatal_safety_failure'), data['jit_metadata'].get('safety_error', data['jit_metadata'].get('error'))
            data['target_after_metadata'] = target_record(target.pid, a.cpu)
            assert Path(f'/proc/{target.pid}/status').read_text().split('TracerPid:')[1].split()[0] == '0'
            check_target()
        if a.calibrate_only:
            run_kernel(case.get('calibration_kernel_timeout', 120))
        else:
            options = ['-duration', '30', '-knob', 'sampling-interval=10', '-cpu-mask', f'{a.companion},{a.cpu}',
                       '-knob', 'enable-stack-collection=false']
            if a.kind == 'hotspots':
                collect = ['-collect-with', 'runsa']
                options += ['-knob', 'enable-driverless-collection=true', '-knob',
                            f'event-config=CPU_CLK_UNHALTED.REF_TSC:sa={HOTSPOTS_EVENT_PERIOD_BASE},INST_RETIRED.ANY:sa={HOTSPOTS_EVENT_PERIOD_BASE}']
            else:
                collect = ['-collect', 'uarch-exploration']
                options += ['-knob', 'pmu-collection-mode=' + ('summary' if a.kind == 'uarch-summary' else 'detailed')]
            command = [str(a.vtune), *collect, '-result-dir', str(a.out / 'result'), '-target-pid', str(target.pid), *options]
            data['collect_argv'] = command
            with (a.out / 'collect.log').open('wb') as log:
                collector = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
                remember_process(collector)
                deadline = time.monotonic() + 60
                while time.monotonic() < deadline and collector.poll() is None:
                    check_target()
                    text = (a.out / 'collect.log').read_text(errors='replace')
                    if 'Collection started' in text or 'Collection resumed' in text:
                        break
                    time.sleep(.1)
                else:
                    raise RuntimeError('hardware collector did not start: ' + (a.out / 'collect.log').read_text(errors='replace')[-4000:])
                run_kernel(min(case.get('kernel_timeout', 28), 28))
                assert collector.poll() is None, 'collector ended before DONE'
                stop_code = owned_command([str(a.vtune), '-command', 'stop', '-r', str(a.out / 'result')],
                                          a.out / 'stop.log', a.out / 'stop.stderr.log', 120)
                assert stop_code == 0, (a.out / 'stop.stderr.log').read_bytes()[-4000:]
                data['collector_exit'] = wait_process(collector, 180, observe=check_target)
                assert data['collector_exit'] == 0
        target.stdin.write(b'D')
        target.stdin.flush()
        target.stdin.close()
        data['target_exit'] = wait_process(target, 60, observe=observe_commands)
        assert data['target_exit'] == 0
        stdout = contents('stdout')
        assert output_matches(stdout, stable_checks(case)), stdout[-4000:]
        gate_lines = contents(case['gate_stream']).splitlines()
        assert gate_lines.count(b'UWVM_PROFILE_READY') == gate_lines.count(b'UWVM_PROFILE_DONE') == 1
        data['kernel_seconds'] = kernel_seconds(stdout.decode(errors='replace'), case)
        validate_kernel_seconds(data['kernel_seconds'], data['guest_window_seconds'])
        data['hardware_duration_eligible'] = 1 <= data['kernel_seconds'] <= 20
        if not a.calibrate_only:
            assert data['hardware_duration_eligible'], 'actual kernel timer outside 1..20 seconds; recalibrate workload'
        data['kernel_qualification_seconds'] = [1, 20]
        lines = stdout.decode().replace('\r', '').splitlines()
        timer = (r'csharp kernel iterations=[0-9]+ ticks=[0-9]+ frequency=[0-9]+' if case['kernel_time'] == 'ticks'
                 else re.escape(case.get('clock_line_prefix', 'Kernel ns: ')) + r'[0-9]+(?:\.[0-9]+)?')
        stable = [line for line in lines if not re.fullmatch(timer, line)
                  and not (case['gate_stream'] == 'stdout' and line in [case['gate_ready'], case['gate_done']])]
        expected = case.get('checks', case.get('expected_stdout', '').splitlines())
        assert stable == expected, ('complete stable stdout mismatch', stable, expected)
        if not a.calibrate_only:
            assert 1 <= data['guest_window_seconds'] < 28, 'kernel outside bounded 1..28 second window'
        assert 'debug-local-ssa' not in contents('stderr').decode(errors='replace'), 'debug IR policy unexpectedly enabled'
        if not a.calibrate_only:
            if a.kind == 'hotspots':
                data['actual_hotspots_period'] = actual_hotspots_period(a.out / 'result/config/runsa.options')
                assert data['actual_hotspots_period']['actual_event_period'] == data['custom_sampling_period']
            data.update(hardware_reports(a.vtune, a.out, a.kind, a.cpu in atom, run_command=owned_command,
                                         target_pid=target.pid))
        assert digest(binary) == data['binary_sha256'] and digest(case['wasm']) == data['wasm_sha256']
        assert digest(public_binary) == data['binary_sha256'] and digest(public_wasm) == data['wasm_sha256']
        _, data['runtime_library_copies_after'] = profile_environment(a.arena, a.profile_libraries)
        assert data['runtime_library_copies_after'] == library_proof, 'runtime dependency copies or aliases changed'
        assert digest(a.manifest) == data['manifest_sha256']
        assert all(digest(path) == checksum for path, checksum in helpers.items())
        assert digest(a.vtune) == data['vtune_sha256'] and digest(setpriv) == data['setpriv_sha256']
        if safety is not None:
            safety.check(force=True)
        success = True
    except BaseException as error:
        data['error'] = repr(error)
        raise
    finally:
        cleanup_errors = []
        data['kernel_alerts'] = safety.events if safety is not None else []
        for action in [*[lambda child=child: terminate_group(child) for child in commands],
                       lambda: terminate_group(collector), lambda: terminate_group(target),
                       lambda: target.stdin.close() if target is not None and target.stdin is not None and not target.stdin.closed else None,
                       *[handle.close for handle in handles], lambda: safety.close() if safety is not None else None]:
            try:
                action()
            except Exception as error:
                cleanup_errors.append(repr(error))
        data['process_ownership'] = {'target': owned_process_record(target),
                                     'collector': owned_process_record(collector),
                                     'commands': [owned_process_record(child) for child in commands]}
        try:
            data['admission_after'] = admission()
            check_admission(before, data['admission_after'])
            assert Path('/proc/sys/kernel/random/boot_id').read_text().strip() == data['boot']
        except Exception as error:
            cleanup_errors.append(repr(error))
        data['cleanup_errors'] = cleanup_errors
        durable_write(a.out / 'receipt.json', json.dumps(data, indent=2, allow_nan=False))
        try:
            sync_artifacts(a.out)
            data['artifacts_synced_before_receipt'] = True
            data['passed'] = success and not cleanup_errors
        finally:
            durable_write(a.out / 'receipt.json', json.dumps(data, indent=2, allow_nan=False))
            os.close(lock)
        if success and not data['passed']:
            raise RuntimeError('ordinary profile cleanup/admission failed: ' + repr(cleanup_errors))
    print(json.dumps({key: data[key] for key in ['passed', 'kind', 'case', 'repository', 'guest_window_seconds']}), flush=True)


if __name__ == '__main__':
    main()

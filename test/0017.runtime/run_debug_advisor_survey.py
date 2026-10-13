#!/usr/bin/env python3
"""Bounded Advisor Survey attach to a real Wasm debugger, SSH Linux only.

Survey is auxiliary analysis, not a hardware-counter or throughput result.
The owned target explicitly permits descendants of this parent to attach,
without changing system ptrace policy. Detach (never Advisor stop) preserves
the stopped guest. Compilation, exit and LLVM teardown are outside collection.
"""
import argparse
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import time

from debug_sampling_safety import SamplingSession, durable_write, sync_artifacts
from run_debug_breakpoint_benchmark import exports
from run_debug_run_cli import Console


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def survey_report_rows(text):
    header = re.search(r'"ID","Function Call Sites and Loops"[^\r\n]*', text)
    assert header, 'Actual Advisor Survey table is required'
    records = csv.DictReader(io.StringIO(text[header.start():]))
    # Advisor gives loops numeric IDs but leaves function IDs empty. Keep
    # those actual function rows too, without accepting progress/footer text.
    rows = [row for row in records if (row.get('ID') or '').isdigit() or row.get('Type') == 'Function']
    for row in rows:
        value = row.get('Self Time', '')
        if re.fullmatch(r'\d+(?:\.\d+)?s', value) and float(value[:-1]) > 0:
            return rows
    raise AssertionError('Survey must contain a nonzero measured self-time row')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('uwvm', 'wasm', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--advisor', type=Path,
                        default=Path('/opt/intel/oneapi/advisor/latest/bin64/advisor'))
    parser.add_argument('--expected-cgroup', required=True)
    parser.add_argument('--cpu', type=int, required=True)
    parser.add_argument('--expected-checksum', type=int, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--feature', action='append', default=[],
                        choices=['memory64','table64','multi-memory','extended-const','tail-call','exceptions',
                                 'function-references','reference-types','gc','simd','relaxed-simd'])
    args = parser.parse_args()
    assert os.sched_getaffinity(0) == {args.cpu}
    assert Path('/proc/self/cgroup').read_text().strip() == '0::' + args.expected_cgroup
    cg = Path('/sys/fs/cgroup' + args.expected_cgroup)
    assert (cg / 'memory.max').read_text().strip() == str(64 << 30)
    assert (cg / 'memory.swap.max').read_text().strip() == '0'
    args.out.mkdir(parents=True, exist_ok=False)
    os.environ.update(TERM='dumb', NO_COLOR='1')
    data = {'passed': False, 'analysis': 'Advisor Survey', 'hardware_counters': False,
            'throughput_sample': False, 'cgroup': args.expected_cgroup,
            'binary_sha256': digest(args.uwvm), 'wasm_sha256': digest(args.wasm),
            'sampler_sha256': digest(__file__), 'advisor_sha256': digest(args.advisor),
            'boot': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
            'scope': 'real _start:0 before clock through post-result profile_end:0; no startup/exit/teardown',
            'expected_checksum': args.expected_checksum}
    data['features'] = args.feature
    target = collector = safety = None
    durable_write(args.out / 'receipt.json', json.dumps(data, indent=2))
    try:
        safety = SamplingSession(args.out, sampled_cpus=[args.cpu])
        # The existing monitor's interval field describes VTune defaults. Do
        # not mislabel Advisor's Survey collection as 10 ms PMU sampling.
        safety_path = args.out / 'sampling-safety.json'
        monitoring = json.loads(safety_path.read_text())
        monitoring.update(tool='Advisor Survey', sampling_interval_ms=None,
                          hardware_sampling=False, maximum_collection_s=30)
        durable_write(safety_path, json.dumps(monitoring, indent=2))
        flags = ['-Raot'] if args.ros else ['-Rcm', 'full', '-Rcc', 'jit']
        argv = [str(args.uwvm.resolve()), '-Rdbg', *flags, '-Rct', '0',
                '-Rllvm-cache-path', 'disable', '-Rclog', 'err',
                *['-WFE-'+feature for feature in args.feature], '--run', str(args.wasm.resolve())]
        # PR_SET_PTRACER authorizes only this existing parent and descendants,
        # including the owned Advisor attach child. Target retains zero caps.
        bootstrap = args.out / 'target-bootstrap.py'
        bootstrap.write_text('import ctypes,os,sys\n'
                             'libc=ctypes.CDLL(None,use_errno=True)\n'
                             'assert libc.prctl(0x59616d61,os.getppid(),0,0,0)==0,ctypes.get_errno()\n'
                             'os.execv(sys.argv[1],sys.argv[1:])\n')
        bootstrap.chmod(0o600)
        launch = ['/usr/bin/setpriv', '--inh-caps=-all', '--ambient-caps=-all',
                  '/usr/bin/python3', str(bootstrap), *argv]
        data.update(target_argv=argv, launch_argv=launch,
                    ptrace_authorization='owned parent PID and its descendants; no system setting changed')
        target = Console(launch)
        target.wait(lambda text: b'(uwvm-debug) ' in text, 0, 120)
        entry = target.command('startw')
        symbols = exports(args.wasm)
        assert f"function={symbols['_start']} byte-offset=0 ".encode() in entry
        end = symbols['profile_end']
        reply = target.command(f'bw 0 {end} 0')
        assert re.search(rb'(?:Breakpoint|breakpoint) \d+', reply) and b'error:' not in reply
        data['target_pid'] = target.pid
        assert Path(f'/proc/{target.pid}/cgroup').read_text().strip() == '0::' + args.expected_cgroup
        assert Path(f'/proc/{target.pid}/exe').resolve() == args.uwvm.resolve()
        data['task_affinities'] = {path.name: sorted(os.sched_getaffinity(int(path.name)))
                                 for path in Path(f'/proc/{target.pid}/task').iterdir()}
        assert all(value == [args.cpu] for value in data['task_affinities'].values())
        status = Path(f'/proc/{target.pid}/status').read_text()
        data['target_capabilities'] = {name: int(value, 16) for name, value in re.findall(
            r'^(Cap(?:Inh|Prm|Eff|Amb)):\s*([0-9a-f]+)$', status, re.M)}
        assert data['target_capabilities'] and not any(data['target_capabilities'].values())
        project = args.out / 'project'
        collect = [str(args.advisor), '--collect=survey', '--target-pid=' + str(target.pid),
                   '--project-dir=' + str(project), '--no-stacks', '--duration=30',
                   '--data-limit=128', '--no-profile-intel-perf-libs',
                   '--search-dir', 'bin:r=' + str(args.uwvm.parent)]
        data['collect_argv'] = collect
        with (args.out / 'collect.log').open('w') as log:
            collector = subprocess.Popen(collect, stdout=log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline and collector.poll() is None:
                safety.check()
                text = (args.out / 'collect.log').read_text(errors='replace')
                if re.search(r'Collection (?:started|resumed)', text, re.I):
                    break
                time.sleep(.1)
            else:
                raise RuntimeError('Advisor collection did not acknowledge attach: ' +
                                   (args.out / 'collect.log').read_text()[-2500:])
            offset = len(target.log)
            os.write(target.fd, b'c\n')
            reply = target.wait(safety.predicate(lambda text: b'Kernel ns: ' in text and
                                re.search(rb'(?:^|\n)\(uwvm-debug\) $', text)), offset, 30)
            times = re.findall(rb'Kernel ns: (\d+)', reply)
            checksums = re.findall(rb'Checksum: ([^\r\n]+)', reply)
            assert len(times) == len(checksums) == 1
            assert int(checksums[0]) == args.expected_checksum
            assert b'stopped: breakpoint' in reply and f'function={end} byte-offset=0 '.encode() in reply
            assert b'Program exited' not in reply and collector.poll() is None
            data.update(profiled_kernel_ms=int(times[0]) / 1e6, checksum=int(checksums[0]))
            actual = re.search(r'advisor -r (.+?) -command stop',
                               (args.out / 'collect.log').read_text(errors='replace'))
            assert actual, 'Advisor must name its actual running result directory'
            running_result = Path(actual[1]).resolve()
            assert running_result.is_relative_to(project.resolve()) and running_result.is_dir()
            data['actual_result_directory'] = str(running_result)
            # Intel documents stop as killing an attached target; use detach.
            detach = [str(args.advisor), '--command=detach', '--result-dir=' + str(running_result)]
            result = subprocess.run(detach, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
            (args.out / 'detach.log').write_bytes(result.stdout)
            data.update(detach_argv=detach, detach_exit=result.returncode)
            assert result.returncode == 0, result.stdout[-2500:]
            data['collect_exit'] = collector.wait(timeout=180)
        assert data['collect_exit'] == 0, (args.out / 'collect.log').read_text()[-2500:]
        assert Path(f'/proc/{target.pid}/exe').resolve() == args.uwvm.resolve()
        assert b'error:' not in target.command('disable')
        assert b'Program exited' in target.command('c')
        target.finish()
        for name, extra in [('survey', []), ('functions', ['--show-functions'])]:
            report = [str(args.advisor), '--report=survey', '--project-dir=' + str(project), '--format=csv', *extra]
            result = subprocess.run(report, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
            (args.out / (name + '.csv')).write_bytes(result.stdout)
            data[name + '_report'] = {'argv': report, 'exit': result.returncode}
            assert result.returncode == 0, result.stdout[-2500:]
            data[name + '_rows'] = survey_report_rows(result.stdout.decode(errors='replace'))
            if name == 'functions':
                assert any(row['Type'] == 'Function' for row in data[name + '_rows']), 'Actual function rows required'
        data['immutable_after'] = {'binary': digest(args.uwvm), 'wasm': digest(args.wasm)}
        assert data['immutable_after'] == {'binary': data['binary_sha256'], 'wasm': data['wasm_sha256']}
        safety.check(force=True)
        data['passed'] = True
    except Exception as error:
        data['error'] = repr(error)
        raise
    finally:
        if collector is not None and collector.poll() is None:
            # Only terminate the collector created here, never another job.
            collector.terminate()
            try:
                collector.wait(timeout=15)
            except subprocess.TimeoutExpired:
                collector.kill()
                collector.wait(timeout=5)
        if target is not None:
            durable_write(args.out / 'transcript.bin', target.log)
            target.close()
        if safety is not None:
            data['kernel_alerts'] = safety.events
            safety.close()
        sync_artifacts(args.out)
        durable_write(args.out / 'receipt.json', json.dumps(data, indent=2))
    print(json.dumps({key: data[key] for key in ('passed', 'analysis', 'checksum', 'binary_sha256')}), flush=True)


if __name__ == '__main__':
    main()

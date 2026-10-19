#!/usr/bin/env python3
"""Recheck existing PerfTimed JVM classes and original Wasm; never compile.

Only the original SSH Linux cgroup/CPU6 may run this bounded checksum oracle.
Historical unrecorded stderr is not reconstructed or relabelled by this run.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shutil
import subprocess
import sys
import time

CG = '0::/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
WASM_SHA = '52505d5526e09f85c7e4b7e9781f38aabcbe7c3c35c3ca7642a280572e6757ee'
SOURCE_SHA = '8b2f6ea68f6701d4b2b04d30dc76db41e58fbd56e26f94003c8ff3b841c21d8e'
CLASS_SHA = '87500ddedabba05c26efdcde8d2efbf85fa67ad6169d5a8efc07a0f5a49fd15b'
WASMTIME_SHA = '21f8e8f994a96d2267afe4a4c06a6302e78aca20e9438afbf01dd443fe32108b'


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def save(path, row):
    path.write_text(json.dumps(row, ensure_ascii=False, indent=2, allow_nan=False) + '\n', encoding='utf-8')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('project', 'helpers', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--java', type=Path, default=Path('/usr/lib/jvm/java-21-openjdk-amd64/bin/java'))
    p.add_argument('--wasmtime', type=Path, default=Path('/home/macromodel/.wasmtime/bin/wasmtime'))
    p.add_argument('--expected-boot', required=True)
    a = p.parse_args()
    assert sys.platform == 'linux' and Path('/proc/self/cgroup').read_text().strip() == CG
    assert 6 in os.sched_getaffinity(0)
    os.sched_setaffinity(0, {6})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    sys.path.insert(0, str(a.helpers.resolve(strict=True)))
    from owned_process import remember_process, terminate_group, owned_process_record
    a.project = a.project.resolve(strict=True)
    a.out = a.out.resolve()
    a.out.mkdir(parents=True, exist_ok=False)
    cg = Path('/sys/fs/cgroup') / CG[3:].lstrip('/')

    def guard():
        assert Path('/proc/self/cgroup').read_text().strip() == CG
        assert Path('/proc/sys/kernel/random/boot_id').read_text().strip() == a.expected_boot
        assert os.sched_getaffinity(0) == {6}
        assert (cg / 'memory.max').read_text().strip() == str(64 << 30)
        assert (cg / 'memory.swap.max').read_text().strip() == '0'
        pressure = (cg / 'memory.events').read_text()
        assert all(int(line.split()[1]) == 0 for line in pressure.splitlines())
        anon = int(dict(line.split() for line in (cg / 'memory.stat').read_text().splitlines())['anon'])
        assert anon < 50 << 30 and shutil.disk_usage(a.out).free >= 512 << 20
        return {'boot': a.expected_boot, 'membership': CG, 'memory_events': pressure,
                'shared_anon_bytes': anon, 'affinity': [6]}

    report = {'passed': False, 'performance_claim': False,
              'scope': 'Fresh independent JVM existing classes and same-Wasm Wasmtime full-output/empty-stderr checksum reference; no historical stderr claim',
              'wasm_sha256': WASM_SHA, 'source_sha256': SOURCE_SHA, 'class_sha256': CLASS_SHA,
              'boot': a.expected_boot, 'runs': {}, 'admission_before': guard()}
    inputs = {}
    classes = a.project / 'target/perf-timed/classes'
    source = a.project / 'src/main/java/uwvm/hll/PerfTimed.java'
    wasm = a.project / 'target/perf-timed/java-perf-timed.wasm'

    def inventory():
        assert classes.is_dir(), 'Original classes missing: do not rebuild'
        return {str(path.relative_to(classes)): sha(path) for path in sorted(classes.rglob('*')) if path.is_file()}

    def invoke(engine, argv):
        folder = a.out / engine
        folder.mkdir(exist_ok=False)
        row = {'passed': False, 'argv': list(map(str, argv)), 'performance_claim': False,
               'wasm_executed': engine == 'wasmtime', 'observations': [],
               'retired': False, 'cleanup_errors': [], 'admission_before': guard()}
        started, last = time.monotonic(), float('-inf')
        child = None
        read_fd = write_fd = None
        try:
            with (folder / 'stdout.log').open('wb') as out, (folder / 'stderr.log').open('wb') as err:
                read_fd, write_fd = os.pipe()
                bootstrap = ('import os,sys; fd=int(sys.argv[1]); '
                             'assert os.read(fd,1)==b"G"; os.close(fd); '
                             'os.execvpe(sys.argv[2],sys.argv[2:],os.environ)')
                env = {'PATH': '/usr/bin:/bin', 'LANG': 'C.UTF-8', 'LC_ALL': 'C.UTF-8', 'TERM': 'dumb', 'NO_COLOR': '1'}
                child = subprocess.Popen([sys.executable, '-c', bootstrap, str(read_fd), *row['argv']],
                                         stdin=subprocess.DEVNULL, stdout=out, stderr=err,
                                         env=env, pass_fds=(read_fd,), start_new_session=True)
                os.close(read_fd)
                read_fd = None
                remember_process(child, force=True)
                row['pid'] = child.pid
                os.write(write_fd, b'G')
                os.close(write_fd)
                write_fd = None
                while True:
                    remember_process(child)
                    now = time.monotonic()
                    if now - last >= .25:
                        last = now
                        current = guard()
                        assert current['memory_events'] == row['admission_before']['memory_events']
                        rss = 0
                        for member in owned_process_record(child)['members']:
                            identity = member['identity']
                            root = Path('/proc') / str(identity['pid'])
                            try:
                                stat = (root / 'stat').read_text(errors='surrogateescape').rpartition(')')[2].split()
                                if int(stat[19]) != identity['start']:
                                    continue
                                assert (root / 'cgroup').read_text().strip() == CG
                                for task in (root / 'task').iterdir():
                                    try:
                                        assert os.sched_getaffinity(int(task.name)) == {6}
                                    except ProcessLookupError:
                                        pass
                                status = (root / 'status').read_text(errors='surrogateescape')
                                value = re.search(r'^VmRSS:\s+([0-9]+)\s+kB$', status, re.M)
                                rss += int(value[1]) * 1024 if value else 0
                            except (FileNotFoundError, ProcessLookupError):
                                continue
                        row['observations'].append({'seconds': now - started, 'owned_rss_bytes': rss,
                                                    'shared_anon_bytes': current['shared_anon_bytes']})
                        assert rss < 4 << 30, 'owned reference RSS exceeded 4GiB'
                        assert all((folder / (name + '.log')).stat().st_size < 1 << 20 for name in ('stdout', 'stderr'))
                    code = child.poll()
                    if code is not None:
                        row['exit'] = code
                        break
                    assert now - started < 120, 'reference deadline'
                    time.sleep(.05)
            stdout, stderr = (folder / 'stdout.log').read_bytes(), (folder / 'stderr.log').read_bytes()
            match = re.fullmatch(rb'java perf timed: iterations=200000000\nKernel ns: ([0-9]+)\nchecksum=4822258612557296351\n', stdout)
            assert code == 0 and stderr == b'' and match and int(match[1]) > 0, (code, stdout, stderr)
            row.update(full_stdout_verified=True, guest_stderr_verified_empty=True,
                       kernel_ns=int(match[1]), iterations=200000000, checksum=4822258612557296351)
            row['passed'] = True
        except BaseException as error:
            row['error'] = repr(error)
        finally:
            for fd in (read_fd, write_fd):
                if fd is not None:
                    os.close(fd)
            if child is not None:
                try:
                    terminate_group(child)
                    row['retired'] = True
                except BaseException as error:
                    row['cleanup_errors'].append(repr(error))
                row['ownership'] = owned_process_record(child)
            for name in ('stdout', 'stderr'):
                path = folder / (name + '.log')
                if path.exists():
                    row[name] = {'path': str(path), 'sha256': sha(path), 'bytes': path.stat().st_size}
            try:
                row['admission_after'] = guard()
                assert row['admission_after']['memory_events'] == row['admission_before']['memory_events']
            except BaseException as error:
                row['cleanup_errors'].append(repr(error))
            owner = row.get('ownership')
            row['all_owned_retired'] = bool(row['retired'] and owner and owner['closed'] and owner['members']
                                           and all(not m['pidfd_retained'] for m in owner['members']))
            row['passed'] = row['passed'] and row['all_owned_retired'] and not row['cleanup_errors']
            row['seconds_including_cleanup'] = time.monotonic() - started
            save(folder / 'receipt.json', row)
        return row

    try:
        assert sha(source) == SOURCE_SHA and sha(wasm) == WASM_SHA
        class_inventory = inventory()
        assert class_inventory['uwvm/hll/PerfTimed.class'] == CLASS_SHA
        assert sha(a.wasmtime) == WASMTIME_SHA
        jdk = a.java.resolve(strict=True).parent.parent
        dependencies = [jdk / name for name in ('release', 'lib/modules', 'lib/server/libjvm.so',
                                                 'lib/libjava.so', 'lib/libjli.so', 'lib/libverify.so')]
        inputs = {str(path.resolve(strict=True)): sha(path) for path in
                  [Path(__file__), a.helpers / 'owned_process.py', source, wasm, a.java, a.wasmtime,
                   *dependencies, *[classes / name for name in class_inventory]]}
        report.update(inputs=inputs, classpath=str(classes), classpath_inventory=class_inventory)
        commands = {'jvm': [a.java, '-Xmx512m', '-XX:ActiveProcessorCount=1', '-cp', classes, 'uwvm.hll.PerfTimed', '200000000'],
                    'wasmtime': [a.wasmtime, 'run', wasm, '200000000']}
        for engine, command in commands.items():
            row = invoke(engine, command)
            row['receipt'] = {'path': str(a.out / engine / 'receipt.json'), 'sha256': sha(a.out / engine / 'receipt.json')}
            report['runs'][engine] = row
            save(a.out / 'report.json', report)
            assert row['all_owned_retired'] and not row['cleanup_errors'], 'reference ownership/guard failed'
        report['passed'] = len(report['runs']) == 2 and all(row['passed'] for row in report['runs'].values())
    except BaseException as error:
        report.update(passed=False, error=repr(error))
    finally:
        try:
            report['admission_after'] = guard()
            report['inputs_after'] = {path: sha(path) for path in inputs}
            report['classpath_inventory_after'] = inventory()
            assert report['inputs_after'] == inputs and report['classpath_inventory_after'] == report.get('classpath_inventory')
            assert report['admission_after']['memory_events'] == report['admission_before']['memory_events']
        except BaseException as error:
            report.update(passed=False, finalization_error=repr(error))
        report['all_owned_retired'] = bool(report['runs']) and all(row['all_owned_retired'] for row in report['runs'].values())
        report['passed'] = report['passed'] and report['all_owned_retired']
        save(a.out / 'report.json', report)
    print(json.dumps({'passed': report['passed'], 'report': str(a.out / 'report.json')}))
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()

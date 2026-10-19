#!/usr/bin/env python3
"""Finite, owned subprocesses in the existing exact Linux 64-GiB cgroup."""
import fcntl
import os
from pathlib import Path
import subprocess
import sys
import time

sys.dont_write_bytecode = True
SUITE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(SUITE))
from build_products import admission, digest
from owned_process import remember_process, terminate_group, owned_process_record
from vtune_reports import durable_json

MEMBERSHIP = ('0::/system.slice/docker-'
              '4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope')


def require(value, message):
    if not value:
        raise RuntimeError(message)


class GuardFailure(RuntimeError):
    pass


def unchanged_admission(first):
    current = admission()  # Also enforces memory.max, swap0 and RLIMIT_CORE=0.
    require(current['membership'] == first['membership'] == MEMBERSHIP, 'existing cgroup membership changed')
    old = dict(line.split() for line in first['memory_events'].splitlines())
    new = dict(line.split() for line in current['memory_events'].splitlines())
    require(all(old.get(key) == new.get(key) for key in ('high', 'max', 'oom', 'oom_kill', 'oom_group_kill')),
            'cgroup memory pressure changed')
    return current


class Session:
    def __init__(self, out, cpu):
        self.before = admission()
        require(self.before['membership'] == MEMBERSHIP, 'original existing Docker cgroup required')
        require(os.uname().machine == 'x86_64', 'native oracle/toolchain require x86-64 Linux')
        require(cpu in os.sched_getaffinity(0), 'requested CPU is outside inherited affinity')
        os.sched_setaffinity(0, {cpu})
        self.cpu, self.out, self.commands = cpu, Path(out), []
        self.boot = Path('/proc/sys/kernel/random/boot_id').read_text().strip()
        self.lock = os.open('/tmp/uwvm-hll-benchmark.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
        try:
            fcntl.flock(self.lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BaseException:
            os.close(self.lock)
            raise

    def guard(self, child):
        try:
            self._guard(child)
        except Exception as error:
            raise GuardFailure(str(error)) from error

    def _guard(self, child):
        remember_process(child)
        unchanged_admission(self.before)
        require(Path('/proc/sys/kernel/random/boot_id').read_text().strip() == self.boot, 'boot changed')
        stat = os.statvfs(self.out)
        require(stat.f_bavail * stat.f_frsize >= 512 << 20, 'disk reserve below 512 MiB')
        group = Path('/sys/fs/cgroup') / MEMBERSHIP[3:].lstrip('/')
        memory = dict(line.split() for line in (group / 'memory.stat').read_text().splitlines())
        require(int(memory['anon']) < 56 << 30, 'cgroup anonymous memory budget exceeded')
        try:
            status = (Path('/proc') / str(child.pid) / 'status').read_text()
            fields = dict(line.split(':', 1) for line in status.splitlines() if ':' in line)
            require(int(fields.get('VmRSS', '0 kB').split()[0]) * 1024 < 8 << 30, 'leader RSS budget exceeded')
        except (FileNotFoundError, ProcessLookupError):
            pass

    def run(self, argv, name, *, cwd=None, env=None, timeout=1800):
        folder = self.out / 'commands' / name
        folder.mkdir(parents=True, exist_ok=False)
        row = {'argv': [str(value) for value in argv], 'cwd': str(cwd) if cwd else None,
               'passed': False, 'stdout': str(folder / 'stdout.log'), 'stderr': str(folder / 'stderr.log')}
        self.commands.append(row)
        child, failure, started, last_guard = None, None, time.monotonic(), 0
        print('RUN ' + name, flush=True)
        try:
            with Path(row['stdout']).open('wb') as out, Path(row['stderr']).open('wb') as err:
                child = subprocess.Popen(row['argv'], cwd=cwd, env=env, stdout=out, stderr=err, start_new_session=True)
                remember_process(child)
                while True:
                    remember_process(child)
                    now = time.monotonic()
                    if now - last_guard >= .5:
                        self.guard(child)
                        last_guard = now
                    code = child.poll()
                    if code is not None:
                        row['exit'] = code
                        break
                    if now - started > timeout:
                        raise subprocess.TimeoutExpired(row['argv'], timeout)
                    time.sleep(.05)
        except BaseException as error:
            failure = error
            row['error'] = f'{type(error).__name__}: {error}'
        finally:
            if child is not None:
                try:
                    terminate_group(child)
                    row['retired'] = True
                except BaseException as error:
                    row['cleanup_error'] = f'{type(error).__name__}: {error}'
                    failure = failure or error
                row['ownership'] = owned_process_record(child)
            row['seconds'] = time.monotonic() - started
            row['passed'] = failure is None and row.get('exit') == 0 and row.get('retired', False)
            for name in ('stdout', 'stderr'):
                path = Path(row[name])
                if path.exists():
                    with path.open('rb') as stream:
                        os.fsync(stream.fileno())
                    row[name + '_sha256'] = digest(path)
            durable_json(folder / 'receipt.json', row)
        if failure is not None:
            raise failure
        require(row['passed'], f'command failed; see {folder}')
        return row

    def close(self):
        os.close(self.lock)

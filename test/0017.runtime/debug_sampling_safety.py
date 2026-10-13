"""Bounded, serialized PMU collection and durable evidence on SSH Linux.

This guard cannot recover a wedged kernel. It stops a collection when the
kernel first reports sampling trouble; it never changes global sysctls or
drivers. Callers remain responsible for stopping only their owned children.
"""
import fcntl
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time


def durable_write(path, payload):
    """Publish an entirely written file, fsync it and its parent directory."""
    path = Path(path)
    if isinstance(payload, str):
        payload = payload.encode()
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + '.', delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def sync_artifacts(directory):
    # Do this before publishing a passed receipt. Interrupted collections
    # keep their initial, explicitly incomplete receipt.
    root = Path(directory)
    directories = [root]
    for path in root.rglob('*'):
        if path.is_symlink():
            continue
        if path.is_dir():
            directories.append(path)
        elif path.is_file():
            with path.open('rb') as stream:
                os.fsync(stream.fileno())
    # VTune raw datasets live below result/data.* and result/db. Publish a
    # passed receipt only after those files and their directory entries too.
    for path in reversed(directories):
        fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)


def sampling_options(kind, pmu_mode, cpu, sampling_interval_ms=10):
    if not isinstance(cpu, int) or cpu < 0:
        raise ValueError('A single verified CPU is required')
    if type(sampling_interval_ms) is not int or not 10 <= sampling_interval_ms <= 100:
        raise ValueError('Sampling interval must be an integer from 10 to 100 ms')
    options = ['-duration', '30', '-knob', 'sampling-interval=' + str(sampling_interval_ms), '-cpu-mask', str(cpu)]
    if kind == 'hotspots':
        options += ['-knob', 'sampling-mode=hw', '-knob', 'enable-stack-collection=false']
    elif kind == 'uarch-exploration':
        options += ['-knob', 'pmu-collection-mode=' + pmu_mode,
                    '-knob', 'enable-stack-collection=false']
    else:
        raise ValueError(kind)
    return options


class SamplingSession:
    ALERT = re.compile(r'perf: interrupt took too long|NMI handler .*took too long|'
                       r'WARNING: kernel stack frame pointer|BUG:|(?:hard|soft) LOCKUP|'
                       r'(?:hard|soft) lockup|rcu.*stall|Out of memory:|oom-kill', re.I)

    def __init__(self, directory, sampled_cpus=None, sampling_interval_ms=10):
        if type(sampling_interval_ms) is not int or not 10 <= sampling_interval_ms <= 100:
            raise ValueError('Sampling interval must be an integer from 10 to 100 ms')
        self.sampling_interval_ms = sampling_interval_ms
        self.directory = Path(directory)
        self.lock = None
        self.cursor = None
        self.last_check = 0
        self.events = []
        self.boot = Path('/proc/sys/kernel/random/boot_id').read_text().strip()
        self.cpus = sorted(os.sched_getaffinity(0))
        self.sampled_cpus = self.cpus if sampled_cpus is None else sorted(set(sampled_cpus))
        if not 1 <= len(self.sampled_cpus) <= 2 or not set(self.cpus) <= set(self.sampled_cpus):
            raise ValueError('Only the target CPU and at most one verified hybrid PMU companion are allowed')
        try:
            if len(self.cpus) != 1:
                raise RuntimeError('A sampler must be pinned to one verified CPU')
            self.lock = os.open('/tmp/uwvm-debug-vtune-' + str(os.getuid()) + '.lock',
                                os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
            fcntl.flock(self.lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self._journal(initial=True)
            durable_write(self.directory / 'sampling-safety.json', json.dumps({
                'boot': self.boot, 'cursor': self.cursor, 'stack_collection': False,
                'sampling_interval_ms': self.sampling_interval_ms, 'maximum_collection_s': 30, 'sampled_cpus': self.sampled_cpus,
                'serialized': True, 'kernel_alerts': []}, indent=2))
        except BaseException:
            self.close()
            raise

    def _journal(self, initial=False):
        argv = ['journalctl', '-k', '-b', '--no-pager', '-o', 'json', '--show-cursor']
        argv += ['-n', '1'] if initial else ['--after-cursor', self.cursor]
        result = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=3)
        if result.returncode:
            raise RuntimeError('Kernel journal monitoring unavailable: ' + result.stderr.decode(errors='replace'))
        cursor = None
        for line in result.stdout.decode().splitlines():
            if line.startswith('-- cursor: '):
                cursor = line[len('-- cursor: '):]
            elif line.startswith('{'):
                row = json.loads(line)
                cursor = row.get('__CURSOR', cursor)
                if not initial and self.ALERT.search(str(row.get('MESSAGE', ''))):
                    self.events.append(row)
        if cursor is not None:
            self.cursor = cursor
        if self.cursor is None:
            raise RuntimeError('Kernel journal has no verifiable cursor')
        if self.events:
            durable_write(self.directory / 'kernel-alerts.json', json.dumps(self.events, indent=2))
            raise RuntimeError('Collection stopped after kernel alert: ' + str(self.events[-1].get('MESSAGE')))

    def check(self, force=False):
        if force or time.monotonic() - self.last_check >= .5:
            if Path('/proc/sys/kernel/random/boot_id').read_text().strip() != self.boot:
                raise RuntimeError('Boot identity changed')
            self._journal()
            self.last_check = time.monotonic()

    def predicate(self, predicate):
        def guarded(output):
            self.check()
            return predicate(output)
        return guarded

    def close(self):
        if self.lock is not None:
            os.close(self.lock)
            self.lock = None

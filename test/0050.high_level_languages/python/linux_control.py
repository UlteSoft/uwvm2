"""Bounded producer/reference processes in the one existing Linux cgroup."""
import hashlib
import json
import os
from pathlib import Path
import resource
import selectors
import shutil
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from owned_process import remember_process, wait_process, terminate_group, owned_process_record

CGROUP = '/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def guard(path):
    if sys.platform != 'linux' or Path('/proc/self/cgroup').read_text().strip() != '0::' + CGROUP:
        raise RuntimeError('producer and references require the existing SSH Linux cgroup')
    cg = Path('/sys/fs/cgroup') / CGROUP.lstrip('/')
    assert (cg / 'memory.max').read_text().strip() == str(64 << 30)
    assert (cg / 'memory.swap.max').read_text().strip() == '0'
    return {'boot': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
            'cgroup': CGROUP, 'affinity': sorted(os.sched_getaffinity(0)),
            'memory_events': (cg / 'memory.events').read_text(),
            'anon': int(dict(line.split() for line in (cg / 'memory.stat').read_text().splitlines())['anon']),
            'disk_free': shutil.disk_usage(path).free}


def invoke(argv, folder, *, cwd=None, env=None, timeout=1800, stdin=None, expected_exit=0):
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=False)
    before = guard(folder)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    started = time.monotonic()
    row = {'argv': list(map(str, argv)), 'cwd': str(cwd) if cwd else None,
           'before': before, 'passed': False, 'memory': []}
    # A start barrier keeps short commands alive until their pidfd is authenticated.
    read_fd, write_fd = os.pipe()
    bootstrap = ('import os,sys; fd=int(sys.argv[1]); '
                 'assert os.read(fd,1)==b"G"; os.close(fd); '
                 'os.execvpe(sys.argv[2],sys.argv[2:],os.environ)')
    child = None
    try:
        with (folder / 'stdout.log').open('wb') as out, (folder / 'stderr.log').open('wb') as err:
            child = subprocess.Popen([sys.executable, '-c', bootstrap, str(read_fd), *map(str, argv)],
                                     pass_fds=(read_fd,), start_new_session=True, cwd=cwd, env=env,
                                     stdin=subprocess.PIPE if stdin is not None else subprocess.DEVNULL,
                                     stdout=out, stderr=err)
            os.close(read_fd)
            read_fd = None
            remember_process(child, force=True)
            os.write(write_fd, b'G')
            os.close(write_fd)
            write_fd = None
            if stdin is not None:
                child.stdin.write(stdin)
                child.stdin.close()
            last = 0

            def observe():
                nonlocal last
                if time.monotonic() - last < .5:
                    return
                last = time.monotonic()
                current = guard(folder)
                assert current['memory_events'] == before['memory_events'], 'new cgroup pressure event'
                assert current['anon'] < 56 << 30, 'shared anonymous budget exhausted'
                assert current['disk_free'] > 2 << 30, 'producer disk reserve reached'
                assert before['disk_free'] - current['disk_free'] < 3 << 30, 'single-stage disk growth exceeds budget'
                origin = int(os.environ.get('UWVM_PYTHON_BASE_FREE', before['disk_free']))
                assert origin - current['disk_free'] < 3 << 30, 'cumulative producer disk growth exceeds budget'
                rss = 0
                ownership = owned_process_record(child)
                for member in ownership['members']:
                    pid = member['identity']['pid']
                    try:
                        text = Path(f'/proc/{pid}/status').read_text()
                        stat = Path(f'/proc/{pid}/stat').read_text().rpartition(')')[2].split()
                        if int(stat[19]) != member['identity']['start']:
                            continue
                        assert Path(f'/proc/{pid}/cgroup').read_text().strip() == '0::' + CGROUP
                        values = dict(line.split(':', 1) for line in text.splitlines() if ':' in line)
                        rss += int(values.get('VmRSS', '0 kB').split()[0]) * 1024
                    except (FileNotFoundError, ProcessLookupError):
                        continue
                assert rss < 8 << 30, 'owned producer RSS budget exceeded'
                row['memory'].append({'seconds': last - started, 'rss_sum': rss,
                                      'shared_anon': current['anon'], 'disk_free': current['disk_free']})

            row['exit'] = wait_process(child, timeout, observe=observe)
            row['passed'] = row['exit'] == expected_exit
    except BaseException as error:
        row['error'] = repr(error)
        raise
    finally:
        for fd in [read_fd, write_fd]:
            if fd is not None:
                os.close(fd)
        if child is not None:
            try:
                terminate_group(child)
                row['retired'] = True
            finally:
                row['ownership'] = owned_process_record(child)
        row['seconds'] = time.monotonic() - started
        row['after'] = guard(folder)
        row['stdout_sha256'] = digest(folder / 'stdout.log')
        row['stderr_sha256'] = digest(folder / 'stderr.log')
        (folder / 'receipt.json').write_text(json.dumps(row, indent=2) + '\n')
    if row['exit'] != expected_exit:
        raise RuntimeError(f'command failed ({row["exit"]}); preserve {folder}')
    return row


def gated(argv, folder, *, env=None, timeout=300):
    """Prove READY/G/DONE/D blocking in a real reference process."""
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=False)
    before = guard(folder)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    started = time.monotonic()
    row = {'argv': list(map(str, argv)), 'before': before, 'passed': False, 'events': []}
    logs = {'stdout': bytearray(), 'stderr': bytearray()}
    read_fd, write_fd = os.pipe()
    bootstrap = ('import os,sys; fd=int(sys.argv[1]); '
                 'assert os.read(fd,1)==b"G"; os.close(fd); '
                 'os.execvpe(sys.argv[2],sys.argv[2:],os.environ)')
    child = subprocess.Popen([sys.executable, '-c', bootstrap, str(read_fd), *map(str, argv)],
                             pass_fds=(read_fd,), start_new_session=True, env=env,
                             stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    os.close(read_fd)
    selector = selectors.DefaultSelector()
    selector.register(child.stdout, selectors.EVENT_READ, 'stdout')
    selector.register(child.stderr, selectors.EVENT_READ, 'stderr')
    try:
        remember_process(child, force=True)
        os.write(write_fd, b'G')
        os.close(write_fd)
        write_fd = None

        def pump(duration, predicate=None):
            deadline = time.monotonic() + duration
            while time.monotonic() < deadline:
                remember_process(child)
                for key, _ in selector.select(min(.05, max(0, deadline - time.monotonic()))):
                    block = os.read(key.fileobj.fileno(), 65536)
                    if block:
                        logs[key.data].extend(block)
                        assert len(logs[key.data]) < 1 << 20
                    else:
                        selector.unregister(key.fileobj)
                current = guard(folder)
                assert current['memory_events'] == before['memory_events']
                if predicate is not None and predicate():
                    return
                if child.poll() is not None and not selector.get_map():
                    if predicate is None:
                        return
                    raise RuntimeError('guest exited before gate marker')
            if predicate is not None:
                raise TimeoutError('reference gate deadline')

        pump(timeout, lambda: b'UWVM_PROFILE_READY\n' in logs['stderr'])
        row['events'].append({'event': 'READY', 'seconds': time.monotonic() - started})
        pump(.2)
        assert child.poll() is None and not logs['stdout'] and b'UWVM_PROFILE_DONE' not in logs['stderr']
        child.stdin.write(b'G')
        child.stdin.flush()
        pump(timeout, lambda: b'UWVM_PROFILE_DONE\n' in logs['stderr'])
        row['events'].append({'event': 'DONE', 'seconds': time.monotonic() - started})
        pump(.2)
        assert child.poll() is None and not logs['stdout']
        assert bytes(logs['stderr']) == b'UWVM_PROFILE_READY\nUWVM_PROFILE_DONE\n'
        child.stdin.write(b'D')
        child.stdin.flush()
        child.stdin.close()
        pump(timeout, lambda: child.poll() is not None and not selector.get_map())
        row['exit'] = child.returncode
        row['passed'] = child.returncode == 0
        assert row['passed']
    except BaseException as error:
        row['error'] = repr(error)
        raise
    finally:
        if write_fd is not None:
            os.close(write_fd)
        try:
            terminate_group(child)
            row['retired'] = True
        finally:
            row['ownership'] = owned_process_record(child)
            selector.close()
            for name, data in logs.items():
                (folder / (name + '.log')).write_bytes(data)
            row['seconds'] = time.monotonic() - started
            row['after'] = guard(folder)
            (folder / 'receipt.json').write_text(json.dumps(row, indent=2) + '\n')
    return row

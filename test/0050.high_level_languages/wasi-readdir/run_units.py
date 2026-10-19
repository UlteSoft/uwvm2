#!/usr/bin/env python3
"""Serial native fd_readdir boundary units, only in the original Linux cgroup.

No UWVM guest/backend build and no old arena source mutation. Each compile uses
one exact before/candidate header overlay and the selected repository's other
headers. Expected old failures are qualified only by the explicit 4074/4096
false-EOF witness, never by an arbitrary nonzero exit.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import signal
import subprocess
import sys
import time

sys.dont_write_bytecode = True
SUITE = Path(__file__).resolve().parent.parent
OWNER_SHA = '37d9d95d078bfbeb8fbca80dc060e2274d1b569589770ed06895825a3a36d9c9'
MEMBERSHIP = ('0::/system.slice/docker-'
              '4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope')
FALSE_EOF = 'EXPECTED_FALSE_EOF used=4074 capacity=4096 complete=29 total=42 remaining_header=22\n'


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def durable(path, value):
    temporary = path.with_suffix(path.suffix + '.tmp')
    with temporary.open('w') as stream:
        json.dump(value, stream, indent=2)
        stream.write('\n')
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)
    fd = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def admission():
    require(os.uname().sysname == 'Linux', 'actual execution must be SSH Linux')
    require(Path('/proc/self/cgroup').read_text().strip() == MEMBERSHIP, 'original existing cgroup required')
    cg = Path('/sys/fs/cgroup') / MEMBERSHIP[3:].lstrip('/')
    require(str(os.getpid()) in (cg / 'cgroup.procs').read_text().split(), 'controller absent from original cgroup')
    require((cg / 'memory.max').read_text().strip() == str(64 << 30), '64 GiB limit changed')
    require((cg / 'memory.swap.max').read_text().strip() == '0', 'swap0 changed')
    return {'boot': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
            'membership': MEMBERSHIP, 'affinity': sorted(os.sched_getaffinity(0)),
            'events': dict(line.split() for line in (cg / 'memory.events').read_text().splitlines()),
            'anon_bytes': int(dict(line.split() for line in (cg / 'memory.stat').read_text().splitlines())['anon'])}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--arena', type=Path, required=True)
    p.add_argument('--sdk', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--cpu', type=int, default=5)
    p.add_argument('--only', choices=['uwvm2', 'uwvm2-stable', 'uwvm2-ros'])
    a = p.parse_args()
    before = admission()
    require(a.cpu == 5 and a.cpu in os.sched_getaffinity(0), 'reserved native-unit CPU5 required')
    os.sched_setaffinity(0, {a.cpu})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    require(digest(SUITE / 'owned_process.py') == OWNER_SHA, 'ownership helper identity changed')
    sys.path.insert(0, str(SUITE))
    from owned_process import remember_process, terminate_group, owned_process_record

    here = Path(__file__).resolve().parent
    a.arena, a.sdk, a.out = a.arena.resolve(strict=True), a.sdk.resolve(strict=True), a.out.resolve()
    a.out.mkdir(parents=True, exist_ok=False)
    report = {'passed': False, 'scope': 'native production API boundary units, not VM/performance qualification',
              'admission_before': before, 'runs': [], 'inputs': {},
              'limits': {'cpu': 5, 'owned_rss_bytes': 2 << 30, 'shared_anon_bytes': 50 << 30,
                         'disk_floor_bytes': 512 << 20, 'guard_period_seconds': .1},
              'ownership_limitation': 'sampled discovery; no claim for helpers fully detaching before first observation'}
    inputs = report['inputs']
    for path in [Path(__file__), here / 'partial_dirent.cc', here / 'candidate-files.json', SUITE / 'owned_process.py']:
        inputs[str(path)] = digest(path)
    compiler = a.sdk / 'bin/clang++'
    inputs[str(compiler)] = digest(compiler)
    sdk_lib = a.sdk / 'lib/x86_64-unknown-linux-gnu'
    environment = dict(os.environ, LD_LIBRARY_PATH=str(sdk_lib) + ':' + str(a.sdk / 'lib'))
    manifest = json.loads((here / 'candidate-files.json').read_text())

    def guard(child=None):
        current = admission()
        require(current['boot'] == before['boot'], 'boot changed')
        require(current['events'] == before['events'], 'cgroup memory events changed')
        require(current['affinity'] == [5], 'controller placement changed')
        require(current['anon_bytes'] < 50 << 30, 'shared anonymous memory reached 50 GiB; yield immediately')
        disk = os.statvfs(a.out)
        require(disk.f_bavail * disk.f_frsize >= 512 << 20, 'disk reserve below 512 MiB')
        if child is not None:
            ownership = remember_process(child)
            rss = 0
            for member in ownership['members']:
                identity = member['identity']
                proc = Path('/proc') / str(identity['pid'])
                try:
                    stat = (proc / 'stat').read_text().rpartition(')')[2].split()
                    if int(stat[19]) != identity['start']:
                        continue
                    status = dict(line.split(':', 1) for line in (proc / 'status').read_text().splitlines() if ':' in line)
                    require(status['Cpus_allowed_list'].strip() == '5', 'owned process placement changed')
                    require((proc / 'cgroup').read_text().strip() == MEMBERSHIP, 'owned process cgroup changed')
                    rss += int(status.get('VmRSS', '0 kB').split()[0]) * 1024
                except (FileNotFoundError, ProcessLookupError):
                    continue
            require(rss < 2 << 30, 'owned aggregate RSS reached 2 GiB')
            return rss
        return 0

    def run(argv, name, *, cwd=None, timeout=120, expected=0, stdout=None, oob=False):
        guard()
        folder = a.out / 'commands' / name
        folder.mkdir(parents=True, exist_ok=False)
        row = {'name': name, 'argv': list(map(str, argv)), 'cwd': str(cwd) if cwd else None,
               'qualified': False, 'expected_exit': expected, 'rss_peak_observed_bytes': 0}
        report['runs'].append(row)
        child = None
        started = time.monotonic()
        try:
            with (folder / 'stdout.log').open('wb') as out, (folder / 'stderr.log').open('wb') as err:
                child = subprocess.Popen(row['argv'], cwd=cwd, env=environment, stdout=out, stderr=err,
                                         start_new_session=True)
                remember_process(child)
                while True:
                    row['rss_peak_observed_bytes'] = max(row['rss_peak_observed_bytes'], guard(child))
                    code = child.poll()
                    if code is not None:
                        row['exit'] = code
                        break
                    require(time.monotonic() - started < timeout, 'command wall timeout')
                    time.sleep(.1)
            output = (folder / 'stdout.log').read_text(errors='replace')
            errors = (folder / 'stderr.log').read_text(errors='replace')
            if oob:
                require(code in [-signal.SIGILL, -signal.SIGABRT, -signal.SIGTRAP], 'OOB call did not take the production explicit trap')
                require(errors.startswith('ABOUT_TO_CALL_OOB\n') and 'OOB call returned' not in errors, 'OOB witness absent')
            else:
                require(code == expected, 'unexpected command exit')
                if stdout is not None:
                    require(output == stdout, 'expected exact false-EOF witness absent')
                elif name.endswith('-boundary'):
                    width = 64 if 'wasm64' in name else 32
                    prefix = f'fd_readdir wasm{width} partial-page contract: PASS checks='
                    require(output.startswith(prefix) and output.endswith(' records=42 tail=22\n'), 'candidate boundary witness absent')
            row['qualified'] = True
        except BaseException as error:
            row['error'] = f'{type(error).__name__}: {error}'
            raise
        finally:
            if child is not None:
                try:
                    terminate_group(child)
                    row['retired'] = child.returncode is not None
                finally:
                    row['ownership'] = owned_process_record(child)
                    require(row['ownership']['closed'], 'owned process retirement incomplete')
            row['seconds'] = time.monotonic() - started
            for filename in ['stdout.log', 'stderr.log']:
                path = folder / filename
                if path.exists():
                    with path.open('rb') as stream:
                        os.fsync(stream.fileno())
                    row[filename + '_sha256'] = digest(path)
            durable(folder / 'receipt.json', row)
            durable(a.out / 'receipt.json', report)
        print(json.dumps({'command': name, 'qualified': row['qualified'], 'exit': row.get('exit'),
                          'seconds': row['seconds']}), flush=True)
        return row

    try:
        for repository in ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']:
            if a.only and repository != a.only:
                continue
            source_root = a.arena / repository
            for width in [32, 64]:
                filename = 'fd_readdir' + ('_wasm64' if width == 64 else '') + '.h'
                item = next(item for item in manifest if item['repository'] == repository and item['path'].endswith('/' + filename))
                original = source_root / item['path']
                require(digest(original) == item['before_sha256'], 'arena original source changed; do not overwrite or silently test a different baseline')
                inputs[str(original)] = digest(original)
                for variant in ['before', 'candidate']:
                    overlay = here / variant / repository / 'src'
                    selected = overlay / item['path'].removeprefix('src/')
                    expected_sha = item['before_sha256' if variant == 'before' else 'after_sha256']
                    require(digest(selected) == expected_sha, 'overlay header identity mismatch')
                    inputs[str(selected)] = expected_sha
                    name = f'{repository}-wasm{width}-{variant}'
                    target = a.out / name
                    argv = [compiler, '-std=c++26', '-O2', '-g', '-stdlib=libc++', '-fuse-ld=lld',
                            '-rtlib=compiler-rt', '-unwindlib=libunwind', '-DUWVM=2',
                            '-DUWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64', f'-DHLL_WASM64={int(width == 64)}',
                            '-L' + str(sdk_lib), '-Wl,-rpath,' + str(sdk_lib), '-Wno-undefined-inline', '-I' + str(overlay)]
                    for include in ['src', 'third-parties/fast_io/include', 'third-parties/bizwen/include', 'third-parties/boost_unordered/include',
                                    'src/uwvm2/imported/wasi/wasip1/func']:
                        argv += ['-I' + str(source_root / include)]
                    argv += [here / 'partial_dirent.cc', '-pthread', '-o', target]
                    run(argv, name + '-compile')
                    inputs[str(target)] = digest(target)
                    fixture = a.out / 'fixtures' / name
                    fixture.mkdir(parents=True)
                    for number in range(40):
                        (fixture / (f'entry-{number:03d}-' + 'x' * 115)).touch()
                    if variant == 'before':
                        run([target, '--expect-old-short-page'], name + '-false-eof', cwd=fixture,
                            timeout=20, expected=42, stdout=FALSE_EOF)
                    else:
                        run([target], name + '-boundary', cwd=fixture, timeout=30)
                        run([target, '--oob'], name + '-oob', cwd=fixture, timeout=10, expected=None, oob=True)
        guard()
        for path, sha in inputs.items():
            require(digest(path) == sha, 'input or tested binary changed: ' + path)
        report['admission_after'] = admission()
        os.sync()
        guard()
        report['passed'] = all(row['qualified'] and row.get('retired') and row['ownership']['closed'] for row in report['runs'])
        require(report['passed'], 'some commands did not qualify')
    except BaseException as error:
        report['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        durable(a.out / 'receipt.json', report)
    print(json.dumps({'passed': report['passed'], 'commands': len(report['runs']), 'out': str(a.out)}), flush=True)


if __name__ == '__main__':
    main()

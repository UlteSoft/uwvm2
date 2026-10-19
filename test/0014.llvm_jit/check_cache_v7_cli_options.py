#!/usr/bin/env python3
"""Unexecuted qualification SOURCE: actual integrated production CLI only.

No fixture-level process-seal/origin implementation is copied. Requires a future
explicit Linux release, original cgroup pins, real built product/fixture hashes,
and a pinned existing owned_process helper. It never compiles or changes sysctls.
The Windows unsupported case is a separate actual guest probe, not Linux PASS.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import resource
import selectors
import signal
import subprocess
import time


OPTION = '--runtime-llvm-jit-cache-seal-process'
ALIAS = '-Rllvm-cache-seal-process'
SEALED = b'[llvm-jit-cache] process-sealed dumpable=0 explicit-cli-request=1 '
EXPECTED = b'CACHE-ORACLE42\n'
OWNER_SHA = '37d9d95d078bfbeb8fbca80dc060e2274d1b569589770ed06895825a3a36d9c9'


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def bound_file(row):
    path = Path(row['path']).resolve(strict=True)
    assert path.is_file() and digest(path) == row['sha256']
    return path


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--inventory', type=Path, required=True)
    ap.add_argument('--expected-inventory-sha256', required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--release-note', required=True)
    a = ap.parse_args()
    assert os.uname().sysname == 'Linux' and a.release_note.strip()
    assert digest(a.inventory) == a.expected_inventory_sha256
    inventory = json.loads(a.inventory.read_text())
    assert inventory['schema'] == 1
    assert Path('/proc/sys/kernel/random/boot_id').read_text().strip() == inventory['boot_id']
    membership = Path('/proc/self/cgroup').read_text().strip()
    assert membership == inventory['cgroup_membership'] and membership.startswith('0::/')
    cg = Path('/sys/fs/cgroup') / membership[3:].lstrip('/')
    assert (cg / 'memory.max').read_text().strip() == '68719476736'
    assert (cg / 'memory.swap.max').read_text().strip() == '0'
    events = (cg / 'memory.events').read_bytes()
    os.sched_setaffinity(0, {8})
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    assert set(inventory['products']) == {'uwvm2', 'uwvm2-stable', 'uwvm2-ros'}
    inputs = {str(a.inventory.resolve()): digest(a.inventory), str(Path(__file__).resolve()): digest(__file__)}
    helper = bound_file(inventory['owned_helper'])
    assert digest(helper) == OWNER_SHA, 'review a new helper explicitly instead of silently changing this test'
    wasm = bound_file(inventory['wasm'])
    strace = bound_file(inventory['strace'])
    for p in (helper, wasm, strace):
        inputs[str(p)] = digest(p)
    binaries = {}
    for repo, product in inventory['products'].items():
        assert product['kind'] == 'production-cli', 'private secure-main wrapper is not this qualification'
        binary = bound_file(product['binary'])
        receipt = bound_file(product['build_receipt'])
        build = json.loads(receipt.read_text())
        assert len(build['commands']) == 4 and all(row['exit'] == 0 for row in build['commands'])
        # Actual production main TU and original 3-TU+link shape; no fourth wrapper.
        assert Path(build['commands'][1]['argv'][-3]).name == 'main.default.cpp'
        binaries[repo] = binary
        inputs[str(binary)] = digest(binary)
        inputs[str(receipt)] = digest(receipt)
    spec = importlib.util.spec_from_file_location('owned_process', helper)
    owned = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(owned)
    out = a.out.resolve()
    arena = Path(inventory['arena']).resolve(strict=True)
    assert out.is_relative_to(arena / 'final')
    out.mkdir(parents=True, exist_ok=False)
    report = {'schema': 1, 'scope': 'actual-production-cli-linux-option-qualification',
              'passed': False, 'errors': [], 'cases': [], 'inputs_before': inputs,
              'release_note': a.release_note, 'performance_claim': False,
              'nonlinux_qualification': 'PENDING: separate real Windows/QGA guest probe SOURCE',
              'admission': {'boot_id': inventory['boot_id'], 'membership': membership,
                            'memory_max': 68719476736, 'swap_max': 0}}

    def interrupted(number, frame):
        for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            signal.signal(sig, signal.SIG_IGN)
        raise KeyboardInterrupt('qualification interrupted by signal ' + str(number))
    for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
        signal.signal(sig, interrupted)

    def global_guard():
        assert Path('/proc/self/cgroup').read_text().strip() == membership
        assert (cg / 'memory.events').read_bytes() == events
        anon = int(dict(line.split() for line in (cg / 'memory.stat').read_text().splitlines())['anon'])
        assert anon < 50 << 30, 'shared anonymous budget'
        fs = os.statvfs(out)
        assert fs.f_bavail * fs.f_frsize >= 4 << 30, 'disk floor'

    def child_setup():
        os.sched_setaffinity(0, {4})
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_AS, (8 << 30, 8 << 30))

    def invoke(name, argv, timeout=300):
        folder = out / name
        folder.mkdir()
        child = None
        stdout, stderr = bytearray(), bytearray()
        row = {'name': name, 'argv': argv, 'passed': False, 'error': None, 'ownership': None}
        selector = selectors.DefaultSelector()
        try:
            global_guard()
            child = subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, start_new_session=True, preexec_fn=child_setup)
            owned.remember_process(child, force=True)
            for stream, buffer in [(child.stdout, stdout), (child.stderr, stderr)]:
                os.set_blocking(stream.fileno(), False)
                selector.register(stream, selectors.EVENT_READ, buffer)
            deadline = time.monotonic() + timeout
            while selector.get_map() or child.poll() is None:
                owned.remember_process(child)
                global_guard()
                assert time.monotonic() < deadline, 'actual command timeout'
                for key, _ in selector.select(.05):
                    data = os.read(key.fd, 65536)
                    if data:
                        key.data.extend(data)
                        assert len(stdout) + len(stderr) <= 8 << 20, 'raw output bound'
                    else:
                        selector.unregister(key.fileobj)
            row['exit'] = child.wait(timeout=1)
        except BaseException as exc:
            row['error'] = repr(exc)
        finally:
            selector.close()
            # A second signal must not interrupt bounded owned retirement.
            saved = {sig: signal.signal(sig, signal.SIG_IGN) for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}
            try:
                if child is not None:
                    owned.terminate_group(child)
                    row['ownership'] = owned.owned_process_record(child)
                    row['retired'] = bool(row['ownership'] and row['ownership']['closed'] and child.returncode is not None)
                    assert row['retired']
            except BaseException as exc:
                row['cleanup_error'] = repr(exc)
            finally:
                if child is not None:
                    for stream in (child.stdout, child.stderr):
                        if stream is not None:
                            stream.close()
                for sig, handler in saved.items():
                    signal.signal(sig, handler)
                for name2, raw in [('stdout', stdout), ('stderr', stderr)]:
                    path = folder / (name2 + '.raw')
                    path.write_bytes(raw)
                    row[name2] = {'path': str(path), 'bytes': len(raw), 'sha256': digest(path)}
                report['cases'].append(row)
                (folder / 'lifecycle.json').write_text(json.dumps(row, indent=2) + '\n')
        assert row['error'] is None and not row.get('cleanup_error') and row.get('retired'), row
        return row, bytes(stdout), bytes(stderr)

    def qualify(row):
        row['passed'] = True
        path = Path(row['stdout']['path']).parent / 'lifecycle.json'
        path.write_text(json.dumps(row, indent=2) + '\n')

    def traced(repo, name, cli, expected_exit, injection=None):
        trace = out / (repo + '-' + name + '.prctl.raw')
        argv = [str(strace), '-f', '-qq', '-s', '256', '-e', 'trace=prctl', '-o', str(trace)]
        if injection:
            # Actual syscall-result injection by a real tracer, not a mirrored
            # process-seal function. Kernel-support semantics are tested normally.
            argv += ['-e', injection]
        row, stdout, stderr = invoke(repo + '-' + name, [*argv, str(binaries[repo]), *cli])
        assert trace.is_file() and trace.stat().st_size <= 1 << 20
        row['trace'] = {'path': str(trace), 'sha256': digest(trace), 'bytes': trace.stat().st_size}
        raw = trace.read_bytes()
        assert row['exit'] == expected_exit
        if injection:
            phase = b'set' if name == 'set-denied' else b'verify'
            assert b'INJECTED' in raw and b'process-seal-failed phase=' + phase + b' ' in stderr
            assert b'PR_SET_DUMPABLE' in raw
            if name == 'get-denied':
                assert b'PR_GET_DUMPABLE' in raw
            assert b'persistent-cache=disabled guest-started=0' in stderr and stdout == b''
            assert SEALED not in stderr
        else:
            assert b'PR_SET_DUMPABLE' not in raw and SEALED not in stderr
            if name in ('cache-path-only', 'guest-argv-cannot-request'):
                assert stdout == EXPECTED and b'object-cache-hit' not in stderr
                assert b'object-cache-store-complete' not in stderr
            if 'help' in name:
                assert OPTION.encode() in stdout + stderr, 'new option absent from actual help'
        qualify(row)

    try:
        for repo in binaries:
            cache = out / (repo + '-cache')
            cache.mkdir(mode=0o700)
            base = ['-Raot', '-Rct', '0', '-Rclog', 'err', '-Rllvm-cache-path', 'path', str(cache)]
            guest = ['--run', str(wasm)]
            traced(repo, 'omitted-help', ['--help'], 0)
            traced(repo, 'cache-path-only', [*base, *guest], 0)
            traced(repo, 'guest-argv-cannot-request', [*base, *guest, OPTION], 0)
            traced(repo, 'flag-before-help', [OPTION, '--help'], 0)
            traced(repo, 'help-before-flag', ['--help', OPTION], 0)
            traced(repo, 'parse-error', [OPTION, '--definitely-invalid-cache-option'], 126)
            traced(repo, 'duplicate-option', [OPTION, ALIAS, *base, *guest], 126)
            traced(repo, 'set-denied', [OPTION, *base, *guest], 126, 'inject=prctl:error=EPERM:when=1')
            traced(repo, 'get-denied', [OPTION, *base, *guest], 126, 'inject=prctl:error=EPERM:when=2')
            for phase, option in [('cold', OPTION), ('warm', ALIAS)]:
                row, stdout, stderr = invoke(repo + '-' + phase, [str(binaries[repo]), option, *base, *guest])
                assert row['exit'] == 0 and stdout == EXPECTED
                assert stderr.count(SEALED) == 1
                assert b'\r' not in stderr and stderr.endswith(b'\n')
                stderr.decode('utf-8', errors='strict')
                prefixes = (b'[llvm-jit-cache] ', b'[llvm-jit-full] ', b'[gc-managed] ')
                assert all(line.startswith(prefixes) for line in stderr.splitlines()), 'unknown guest/engine stderr'
                if phase == 'cold':
                    assert re.search(rb'object-cache-store-complete[^\n]* status=ok(?: |\n)', stderr), 'no authenticated actual cold publication'
                else:
                    assert re.search(rb'object-cache-hit[^\n]* signature_verified=1(?: |\n)', stderr), 'no authenticated actual warm hit'
                qualify(row)
        global_guard()
        assert len(report['cases']) == 33 and all(row['passed'] for row in report['cases'])
        report['passed'] = True
    except BaseException as exc:
        report['errors'].append(repr(exc))
    finally:
        for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            signal.signal(sig, signal.SIG_IGN)
        try:
            report['inputs_after'] = {path: digest(path) for path in inputs}
            assert report['inputs_after'] == inputs, 'bound actual inputs changed'
        except BaseException as exc:
            report['passed'] = False
            report['errors'].append('input audit: ' + repr(exc))
        report['memory_events_before'] = events.decode()
        try:
            report['memory_events_after'] = (cg / 'memory.events').read_text()
            assert report['memory_events_before'] == report['memory_events_after'], 'memory pressure events changed'
        except BaseException as exc:
            report['passed'] = False
            report['errors'].append('final cgroup audit: ' + repr(exc))
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

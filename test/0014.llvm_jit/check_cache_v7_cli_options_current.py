#!/usr/bin/env python3
"""Unexecuted CURRENT SUCCESSOR: actual integrated production CLI only.
The original recovered future33 Source file is preserved unchanged.

No fixture-level process-seal/origin implementation is copied. Requires a future
explicit Linux release, original cgroup pins, real built product/fixture hashes,
and a pinned existing owned_process helper. It never compiles or changes sysctls.
The Windows unsupported case is a separate actual guest probe, not Linux PASS.
"""

# Future Linux only. The keeper admits this direct entry/Python/stdlib before
# startup; no running source can retroactively authenticate its interpreter.
if '__cache_source_context__' not in globals():
    import sys as _source_sys
    if _source_sys.flags.optimize != 0:
        raise RuntimeError('qualification requires unoptimized startup')
    assert __name__ == '__main__' and _source_sys.platform == 'linux'
    assert isinstance(globals().get('__uwvm_entry_captured_bytes__'), bytes), 'use the admitted captured-entry keeper literal'
    assert globals().get('__uwvm_entry_keeper_identity__') == 'cae2c05f7df6e9a2a94a84f5333eeef5c1946d96ea8b54cfe59eee5e17d23d21'
    assert _source_sys.flags.isolated and _source_sys.flags.no_site and _source_sys.flags.safe_path, 'start the admitted entry with -I -S'
    import os as _source_os
    import json as _source_json
    import hashlib as _source_hashlib
    import stat as _source_stat
    import types as _source_types
    import importlib.machinery as _source_machinery
    from pathlib import Path as _SourcePath
    def _source_read_once(path, cap):
        assert path.is_absolute() and path.resolve(strict=True) == path
        fd = _source_os.open(path, _source_os.O_RDONLY | _source_os.O_CLOEXEC | _source_os.O_NOFOLLOW)
        try:
            first = _source_os.fstat(fd)
            assert _source_stat.S_ISREG(first.st_mode) and first.st_uid == _source_os.geteuid() and first.st_size <= cap
            raw = bytearray()
            while len(raw) <= cap:
                part = _source_os.read(fd, min(65536, cap + 1 - len(raw)))
                if not part:
                    break
                raw.extend(part)
            last = _source_os.fstat(fd)
            identity = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)
            assert identity(first) == identity(last) and len(raw) == first.st_size <= cap
            return bytes(raw)
        finally:
            _source_os.close(fd)
    _guard_path = _SourcePath(_source_os.environ['UWVM2_CACHE_EXECUTION_GUARD'])
    _guard_bytes = _source_read_once(_guard_path, 1 << 20)
    assert _source_hashlib.sha256(_guard_bytes).hexdigest() == _source_os.environ['UWVM2_CACHE_EXECUTION_GUARD_SHA256']
    _guard = _source_json.loads(_guard_bytes)
    _membership = _SourcePath('/proc/self/cgroup').read_text().strip()
    assert _guard['release_note'].strip() and _membership == _guard['membership'] and _membership.startswith('0::/') and '\n' not in _membership
    _cgroup = (_SourcePath('/sys/fs/cgroup') / _membership[3:].lstrip('/')).resolve(strict=True)
    assert _cgroup.stat().st_ino == _guard['cgroup_inode']
    assert _SourcePath('/proc/sys/kernel/random/boot_id').read_text().strip() == _guard['boot_id']
    assert (_cgroup / 'memory.max').read_text().strip() == '68719476736'
    assert (_cgroup / 'memory.swap.max').read_text().strip() == '0'
    _repository = _SourcePath(__file__).resolve(strict=True).parents[2]
    _helper_path = _repository / 'test/0014.llvm_jit/cache_cli_process.py'
    _helper_bytes = _source_read_once(_helper_path, 2 << 20)
    assert _source_hashlib.sha256(_helper_bytes).hexdigest() == '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7'
    _prefix = '_uwvm_cache_' + _source_os.urandom(16).hex()
    _helper_name = _prefix + '.cache_cli_process'
    assert _prefix not in _source_sys.modules and _helper_name not in _source_sys.modules
    _package = _source_types.ModuleType(_prefix)
    _package.__package__, _package.__path__ = _prefix, []
    _package.__spec__ = _source_machinery.ModuleSpec(_prefix, loader=None, is_package=True)
    _helper = _source_types.ModuleType(_helper_name)
    _helper.__file__, _helper.__package__, _helper.__cached__ = str(_helper_path), _prefix, None
    _helper.__spec__ = _source_machinery.ModuleSpec(_helper_name, loader=None, origin=str(_helper_path))
    _helper.__spec__.has_location = True
    _source_sys.modules[_prefix], _source_sys.modules[_helper_name] = _package, _helper
    _package.cache_cli_process = _helper
    try:
        # Exactly the SAME captured bytes which passed the hard Source SHA.
        exec(compile(_helper_bytes, str(_helper_path), 'exec', dont_inherit=True, optimize=0), _helper.__dict__, _helper.__dict__)
        __cache_source_context__ = _helper.admit_source_context(_repository, _guard, _helper, _helper_bytes, _package)
        __cache_source_context__.bind_entry(globals())
    except BaseException:
        if '__cache_source_context__' in globals():
            __cache_source_context__.close(audit=False)
        for _name, _owned in ((_helper_name, _helper), (_prefix, _package)):
            if _source_sys.modules.get(_name) is _owned:
                del _source_sys.modules[_name]
        raise
else:
    __cache_source_context__.assert_loaded_scope(globals())


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
_PIPE_SOURCE = Path(__file__).with_name('cache_cli_process.py')
assert hashlib.sha256(_PIPE_SOURCE.read_bytes()).hexdigest() == '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7'
_captured_local_module = __cache_source_context__.load('cache_cli_process')
BoundedCli = _captured_local_module.BoundedCli


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
    assert cg.resolve(strict=True).stat().st_ino == inventory['execution_guard']['cgroup_inode']
    assert inventory['execution_guard']['membership'] == membership
    assert inventory['execution_guard']['boot_id'] == inventory['boot_id']
    # Retain keeper placement; actual child CPU comes from execution_guard.
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
    owned = __cache_source_context__.load_path(helper, OWNER_SHA)
    out = a.out.resolve()
    arena = Path(inventory['arena']).resolve(strict=True)
    assert out.is_relative_to(arena / 'final')
    out.mkdir(parents=True, exist_ok=False)
    runner = BoundedCli(inventory['execution_guard'], out, Path(__file__).resolve().parents[2])
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

    def invoke(name, argv, timeout=300):
        result = runner.execute(name, argv, timeout=timeout, address_space_limit_bytes=8 << 30)
        lifecycle_path = Path(result.lifecycle['path'])
        row = json.loads(lifecycle_path.read_text())
        assert row['passed'] and row['natural_exit_observed'] and row['retired']
        assert not row['ownership']['signals'] and row['ownership']['closed']
        assert all(not x['pidfd_retained'] for x in row['ownership']['members'])
        assert all(x['original_identity_absent_or_reused'] for x in row['post_retirement_identities'])
        row.update(name=name, exit=result.returncode, passed=False, case_oracle_qualified=False,
                   lifecycle_receipt=result.lifecycle)
        report['cases'].append(row)
        return row, result.stdout, result.stderr

    def qualify(row):
        row['passed'] = True
        row['case_oracle_qualified'] = True
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
        assert len(report['cases']) == 33 and all(row['passed'] and row['case_oracle_qualified'] for row in report['cases'])
        runner.guard()
        assert digest(_PIPE_SOURCE) == '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7'
        report['passed'] = True
        report['strict_lifecycle_successor'] = True
        report['sampled_rss_full_lifetime_bound_claimed'] = False
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
    try:
        raise SystemExit(main())
    finally:
        __cache_source_context__.close()

#!/usr/bin/env python3
"""SOURCE: real unsupported-backend/DBG refusal, distinct from positive replay.

Uses the original fd_write CACHE-ORACLE42 fixture. No fake Runtime admission or
fixture-level PR_SET occurs. Debugger runs keep default dumpability for ptrace.
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
import json
from pathlib import Path
import sys

HELPER_SHA = '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7'
REPOSITORY_VARIANT = 'uwvm2-stable'
EXPECTED = b'CACHE-ORACLE42\n'


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--binary-sha256', required=True)
    p.add_argument('--wasm', type=Path, required=True)
    p.add_argument('--wasm-sha256', required=True)
    p.add_argument('--execution-guard', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    assert sys.platform == 'linux'
    helper = Path(__file__).with_name('cache_cli_process.py')
    assert digest(helper) == HELPER_SHA
    _captured_local_module = __cache_source_context__.load('cache_cli_process')
    BoundedCli = _captured_local_module.BoundedCli
    HIT = _captured_local_module.HIT
    STORE = _captured_local_module.STORE
    require_sealed = _captured_local_module.require_sealed
    assert digest(a.binary) == a.binary_sha256 and digest(a.wasm) == a.wasm_sha256
    a.binary, a.wasm = a.binary.resolve(strict=True), a.wasm.resolve(strict=True)
    a.out = a.out.resolve()
    a.out.mkdir(parents=False, exist_ok=False)
    runner = BoundedCli(json.loads(a.execution_guard.read_text()), a.out, Path(__file__).resolve().parents[2])
    cases = [('debugger', ['-Rdbg'], True)]
    if REPOSITORY_VARIANT == 'uwvm2':
        cases += [('lazy', ['-Rjit'], False),
                  ('lazy-verification', ['-Rcm', 'lazy+verification', '-Rcc', 'jit'], False),
                  ('tiered-no-t0', ['-Rtiered', '-Rtiered-disable-t0'], False)]
    rows = []
    for name, flags, debug in cases:
        cache = a.out / (name + '-cache')
        cache.mkdir(mode=0o700)
        for phase in ('first', 'second'):
            command = [str(a.binary), *flags, '-Rct', '0', '-Rclog', 'err',
                       '-Rllvm-cache-path', 'path', str(cache)]
            if not debug:
                command += ['-Rllvm-cache-seal-process']
            command += ['--run', str(a.wasm)]
            result = runner.execute(name + '--' + phase, command, timeout=120,
                interaction='debug-continue-quit' if debug else None)
            raw = result.stdout + result.stderr
            assert result.returncode == 0
            if debug:
                assert raw.count(EXPECTED) == 1 and b'Program exited normally.' in raw
                assert b'process-sealed dumpable=0' not in raw, 'DBG refusal must not acquire ptrace-changing process seal'
            else:
                assert result.stdout == EXPECTED
                require_sealed(result.stderr)
            assert not HIT.search(raw) and b'object-cache-hit' not in raw and not STORE.search(raw)
            assert not any(cache.rglob('*.uwvm-ljc')), 'unsupported backend/DBG published an object'
            rows.append(dict(name=name, phase=phase, command=command, returncode=result.returncode,
                lifecycle_receipt=result.lifecycle, guest_semantics_normal_exit=True,
                cache_refusal_qualified=True, persistent_cache_hit_qualified=False,
                debugger_process_policy_changed=False if debug else None))
    assert len(rows) == len(cases) * 2 and rows
    assert digest(helper) == HELPER_SHA and digest(a.binary) == a.binary_sha256 and digest(a.wasm) == a.wasm_sha256
    report = dict(passed=True, repository_variant=REPOSITORY_VARIANT, rows=rows,
        completed=len(rows), planned=len(cases) * 2, cache_format_version=7,
        cache_refusal_qualified=True, persistent_cache_hit_qualified=False,
        positive_replay_still_mandatory=True, hardware_efficiency_qualified=False,
        scope='only listed actual refusal modes; not a positive cache, full DBG or performance qualification')
    (a.out / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    finally:
        __cache_source_context__.close()

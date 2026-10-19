#!/usr/bin/env python3
"""SOURCE: capture original production CLI; full and refusal contracts are separate."""

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
import shlex
import sys

HELPER_SHA = '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7'


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('self-sha256', 'guard-sha256', 'python-sha256'):
        p.add_argument('--' + name, required=True)
    for name in ('guard', 'python', 'product', 'wasm', 'cwd', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--runtime-args-hex', required=True)
    p.add_argument('--cache-args-hex', required=True)
    p.add_argument('--label', required=True)
    p.add_argument('--contract', choices=('full', 'refusal', 'auth-refusal'), required=True)
    a = p.parse_args()
    assert sys.platform == 'linux' and digest(__file__) == a.self_sha256
    # The keeper MUST also verify interpreter/closure before starting it. This
    # in-process check binds a correctly staged run; it cannot self-authenticate
    # an interpreter which has already executed this source.
    assert a.python.is_absolute() and a.python.resolve(strict=True) == Path(sys.executable).resolve(strict=True)
    assert digest(a.python) == a.python_sha256
    assert digest(a.guard) == a.guard_sha256
    helper = Path(__file__).with_name('cache_cli_process.py')
    assert digest(helper) == HELPER_SHA
    _captured_local_module = __cache_source_context__.load('cache_cli_process')
    BoundedCli = _captured_local_module.BoundedCli
    require_sealed = _captured_local_module.require_sealed
    HIT = _captured_local_module.HIT
    STORE = _captured_local_module.STORE
    output = a.output.resolve()
    assert not output.exists() and output.parent.is_dir()
    runner = BoundedCli(json.loads(a.guard.read_text()), output.parent, Path(__file__).resolve().parents[2])
    def words(raw):
        assert len(raw) <= 1 << 20
        return shlex.split(bytes.fromhex(raw).decode('utf-8', errors='surrogateescape'), posix=True)
    runtime, cache = words(a.runtime_args_hex), words(a.cache_args_hex)
    def option(name, value):
        return name in runtime and runtime.count(name) == 1 and runtime[runtime.index(name) + 1] == value
    full = '-Raot' in runtime or (option('-Rcm', 'full') and option('-Rcc', 'jit'))
    restricted = '-Rjit' in runtime or '-Rtiered' in runtime or (
        ('-Rcm' in runtime and runtime[runtime.index('-Rcm') + 1] in ('lazy', 'lazy+verification')) and option('-Rcc', 'jit'))
    unsafe = any(x in cache for x in ('--runtime-llvm-jit-cache-no-sign', '--runtime-llvm-jit-cache-no-verify'))
    assert not any(x in runtime for x in ('-Rdbg', '-Wpre', '-Rllvm-cache-seal-process', '--runtime-llvm-jit-cache-seal-process'))
    if a.contract == 'full':
        assert full and not restricted and not unsafe, 'positive replay requires actual full signed CLI'
    elif a.contract == 'refusal':
        assert restricted and not full and not unsafe, 'restricted backend refusal must exercise that actual backend'
    else:
        assert full and not restricted and unsafe, 'authentication refusal must actually request a legacy unsafe policy'
    if '-Rct' not in runtime:
        runtime += ['-Rct', '0']
    def entries(root):
        return {str(path.relative_to(root)): digest(path) for path in root.rglob('*.uwvm-ljc')}
    cache_root, prior = None, None
    if a.contract != 'full':
        index = next(i for i, x in enumerate(cache) if x in ('--runtime-llvm-jit-cache-path', '-Rllvm-cache-path'))
        assert cache[index + 1] == 'path'
        cache_root = Path(cache[index + 2])
        if not cache_root.is_absolute():
            cache_root = a.cwd / cache_root
        prior = entries(cache_root)
    product, wasm = a.product.resolve(strict=True), a.wasm.resolve(strict=True)
    before = {str(x): digest(x) for x in (product, wasm, a.guard, a.python, Path(__file__), helper)}
    command = [str(product), *runtime, *cache, '-Rllvm-cache-seal-process', '--run', str(wasm)]
    result = runner.execute(a.label, command, cwd=a.cwd.resolve(strict=True), timeout=180)
    output.write_bytes(result.stdout + result.stderr)
    require_sealed(result.stderr)
    assert result.returncode == 0, 'original Guest semantic control did not exit normally'
    raw = result.stdout + result.stderr
    if a.contract != 'full':
        assert not HIT.search(raw) and b'object-cache-hit' not in raw and not STORE.search(raw)
        assert entries(cache_root) == prior, 'refused configuration published or modified an object'
        if a.contract == 'auth-refusal':
            assert b'origin-authentication-required' in raw, 'actual v7 mandatory authentication decision absent'
    assert {path: digest(path) for path in before} == before
    receipt = dict(actual_original_cli=True, actual_cli_process_sealed=True, cache_format_version=7,
        contract=a.contract, persistent_cache_hit_qualified=False,
        cache_refusal_qualified=a.contract != 'full', guest_semantics_normal_exit=True,
        scope='lifecycle/refusal only; C++ full caller must check cold publication and verified warm hits',
        inputs=before, command=command, lifecycle=result.lifecycle, output_sha256=digest(output),
        returncode=result.returncode, cache_before=prior,
        cache_after=entries(cache_root) if cache_root is not None else None)
    output.with_suffix(output.suffix + '.launch.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    finally:
        __cache_source_context__.close()

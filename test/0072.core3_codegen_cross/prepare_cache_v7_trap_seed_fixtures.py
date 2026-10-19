#!/usr/bin/env python3
"""SOURCE: new explicit cache-replay seed variants; original cases stay unchanged.

All actual imports/generation/tool execution require the released Linux cgroup.
The new Wasm is explicitly not byte-identical to historical negative fixtures.
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
CASES_SOURCE_SHA = '887212e12b97f3501611ceb6d45714da6b2f8cdcc7525175e2d4e263626309af'


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('config', type=Path)
    p.add_argument('output', type=Path)
    a = p.parse_args()
    assert sys.platform == 'linux'
    config = json.loads(a.config.read_text())
    helper = Path(__file__).resolve().parents[1] / '0014.llvm_jit/cache_cli_process.py'
    source = Path(__file__).with_name('high_memory_cases.py')
    assert digest(helper) == HELPER_SHA and digest(source) == CASES_SOURCE_SHA
    tool = Path(config['wasm_tools'])
    assert tool.is_absolute() and digest(tool) == config['wasm_tools_sha256']
    a.output = a.output.resolve()
    a.output.mkdir(parents=False, exist_ok=False)
    _captured_local_module = __cache_source_context__.load('cache_cli_process')
    BoundedCli = _captured_local_module.BoundedCli
    runner = BoundedCli(config['execution_guard'], a.output, Path(__file__).resolve().parents[2])
    # Both imported Source inputs have been pinned and Linux placement admitted.
    _captured_local_module = __cache_source_context__.load('high_memory_cases')
    cache_replay_cases = _captured_local_module.cache_replay_cases
    fixtures = []
    for item in cache_replay_cases():
        name = item['name']
        assert Path(name).name == name
        original, wat, wasm = (a.output / (name + suffix) for suffix in ('.original.wat', '.wat', '.wasm'))
        original.write_text(item['original_wat'] + '\n')
        wat.write_text(item['wat'] + '\n')
        for stage, argv in [('parse', [str(tool), 'parse', str(wat), '-o', str(wasm)]),
                            ('validate', [str(tool), 'validate', '--features', 'all', str(wasm)])]:
            result = runner.execute(name + '--' + stage, argv, timeout=60)
            assert result.returncode == 0
        fixtures.append({k: v for k, v in item.items() if k not in ('wat', 'original_wat')} | dict(
            path=str(wasm), sha256=digest(wasm), wat_sha256=digest(wat),
            original_wat_path=str(original), original_wat_sha256=digest(original),
            original_wasm_byte_identity_claimed=False))
    assert fixtures and digest(helper) == HELPER_SHA and digest(source) == CASES_SOURCE_SHA
    assert digest(tool) == config['wasm_tools_sha256']
    report = dict(fixtures=fixtures, validated_with_all_features=True,
        code_pins={str(helper): digest(helper), str(source): digest(source), str(Path(__file__).resolve()): digest(__file__)},
        scope='new appended seed export; unchanged original _start and OOB operations; no Guest execution yet')
    (a.output / 'fixtures.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    finally:
        __cache_source_context__.close()

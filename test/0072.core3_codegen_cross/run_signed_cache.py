#!/usr/bin/env python3
"""Prove signed cache reuse in three fresh processes per selected CLI case."""

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
import json
import os
from pathlib import Path
import re
import subprocess
import time
import hashlib
import sys
_PIPE_HELPER = Path(__file__).resolve().parents[1] / '0014.llvm_jit/cache_cli_process.py'
assert hashlib.sha256(_PIPE_HELPER.read_bytes()).hexdigest() == '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7', 'review changed bounded PIPE Source'
_captured_local_module = __cache_source_context__.load('cache_cli_process')
BoundedCli = _captured_local_module.BoundedCli
require_cache_phase = _captured_local_module.require_cache_phase

_captured_local_module = __cache_source_context__.load('run_matrix')
digest = _captured_local_module.digest
save = _captured_local_module.save
verify = _captured_local_module.verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    args = parser.parse_args()
    config = json.loads(args.config.read_text())

    verify(config['pins'])
    qualification_path = Path(config['product_qualification'])
    qualification_hash = digest(qualification_path)
    qualification = json.loads(qualification_path.read_text())
    assert qualification['passed']
    assert qualification['product_sha256'] == digest(config['product'])
    dependencies = qualification['actual_dependency_pins']
    verify(dependencies)
    manifest_path = Path(config['fixtures'])
    manifest_hash = digest(manifest_path)
    manifest = json.loads(manifest_path.read_text())
    verify(manifest['code_pins'])
    fixtures = [f for f in manifest['fixtures'] if f['name'] in config['cases']]
    assert {f['name'] for f in fixtures} == set(config['cases'])
    assert all(f.get('expected_trap') is None for f in fixtures)
    assert all(p['backend'] == 'llvm-jit' for p in config['profiles'])
    fixture_pins = {f['path']: f['sha256'] for f in fixtures}
    verify(fixture_pins)
    code_pins = {str(p.resolve()): digest(p) for p in Path(__file__).parent.glob('*.py')}
    code_pins[str(_PIPE_HELPER)] = digest(_PIPE_HELPER)
    output = Path(config['output'])
    output.mkdir(exist_ok=False)
    runner = BoundedCli(config['execution_guard'], output, Path(__file__).resolve().parents[2])
    guard_source = Path(config['cgroup_guard']).resolve(strict=True)
    assert config['pins'].get(str(guard_source)) == digest(guard_source), 'pin actual cgroup guard Source before execution'
    guard_run = runner.execute('cgroup-guard-entry', ['/usr/bin/bash', str(guard_source)], timeout=30)
    assert guard_run.returncode == 0
    assert config['cases'] and config['profiles'], 'empty positive replay plans cannot PASS'
    rows = []
    planned = len(config['profiles']) * len(fixtures) * 6
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')
    for profile in config['profiles']:
        actual_mode = profile['argv']
        assert '-Raot' in actual_mode or ('-Rcc' in actual_mode and 'jit' in actual_mode and '-Rcm' in actual_mode and 'full' in actual_mode)
        assert not any(x in actual_mode for x in ('lazy','tiered','-Rdbg','-Wpre','-Rllvm-cache-no-sign','-Rllvm-cache-no-verify')), 'current origin excludes this positive replay profile'
        for fixture in fixtures:
            for path_mode in ['default', 'explicit']:
                work = output / (profile['name'] + '--' + fixture['name'] + '--' + path_mode)
                work.mkdir()
                env = dict(os.environ)
                for name in ['LD_PRELOAD', 'LD_AUDIT', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT']:
                    env.pop(name, None)
                env['XDG_CACHE_HOME'] = str(work / 'xdg')
                prefix = list(config.get('prefix', []))
                guest_root = None
                if prefix:
                    loader = Path(prefix[prefix.index('-L') + 1])
                    guest_root = work / 'guest-root'
                    guest_root.mkdir()
                    for directory in ['lib', 'lib64', 'usr']:
                        if (loader / directory).exists():
                            (guest_root / directory).symlink_to(loader / directory, target_is_directory=True)
                    prefix[prefix.index('-L') + 1] = str(guest_root)
                    prefix += ['-U', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT',
                               '-U', 'LD_PRELOAD', '-U', 'LD_AUDIT',
                               '-E', 'XDG_CACHE_HOME=' + env['XDG_CACHE_HOME']]
                namespace = 'uwvm2ros' if config['ros'] else 'uwvm2'
                cache = work / ('xdg/' + namespace + '/llvm-jit' if path_mode == 'default' else 'cache')
                flags = [] if path_mode == 'default' else ['-Rllvm-cache-path', 'path', str(cache)]
                original = None
                for phase in ['cold', 'warm', 'warm-again']:
                    log, run = work / (phase + '.compiler.log'), work / (phase + '.run.log')
                    argv = [*prefix, config['product'], *profile['argv'], '-Rct', '0', *flags,
                            '-Rllvm-cache-seal-process', '-Rclog', 'err', *fixture['argv'], '--run', fixture['path']]
                    started = time.monotonic()
                    result = runner.execute(profile['name'] + '--' + fixture['name'] + '--' + path_mode + '--' + phase,
                                            argv, cwd=work, env=env, timeout=config.get('timeout', 300))
                    log.write_bytes(result.stderr)
                    run.write_bytes(result.stdout + result.stderr)
                    require_cache_phase(result.stderr, phase)
                    physical_log = log
                    if not physical_log.exists() and guest_root is not None:
                        physical_log = guest_root / str(log).lstrip('/')
                    assert result.returncode == 0, (phase, run.read_text(errors='replace')[-4000:])
                    text = ansi.sub('', physical_log.read_text())
                    hits = [line for line in text.splitlines() if 'object-cache-hit module=' in line]
                    roots = [cache] + ([guest_root / str(cache).lstrip('/')] if guest_root is not None else [])
                    entries = {}
                    for root in roots:
                        for path in root.rglob('*.uwvm-ljc'):
                            key = str(path.relative_to(root))
                            assert key not in entries
                            entries[key] = digest(path)
                    assert entries, (phase, text[-4000:])
                    if phase == 'cold':
                        assert not hits and re.search(r'object-cache-store-complete .* status=ok\b', text)
                        original = entries
                    else:
                        assert hits and all('signature_verified=1' in line for line in hits)
                        assert entries == original, 'Cache payload changed on authenticated hit'
                    rows.append(dict(passed=True, profile=profile['name'], fixture=fixture['name'],
                        path_mode=path_mode, phase=phase, exit=result.returncode, argv=argv,
                        seconds=time.monotonic() - started, signed_cache_hits=len(hits),
                        physical_compiler_log=str(physical_log), compiler_log_sha256=digest(physical_log),
                        run_log_sha256=digest(run), physical_cache_roots=[str(p) for p in roots], cache_entries=entries,
                        lifecycle_receipt=result.lifecycle, cache_format_version=7, actual_cli_process_sealed=True))
                    save(output / 'checkpoint.json', dict(state='running', planned=planned, completed=len(rows)))
                print('PASS signed cache', config['arch'], profile['name'], fixture['name'], path_mode, flush=True)
    for pins in [config['pins'], dependencies, fixture_pins, manifest['code_pins'], code_pins]:
        verify(pins)
    assert digest(qualification_path) == qualification_hash
    assert digest(manifest_path) == manifest_hash
    assert len(rows) == planned and planned > 0
    assert digest(_PIPE_HELPER) == '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7'
    guard_run = runner.execute('cgroup-guard-exit', ['/usr/bin/bash', str(guard_source)], timeout=30)
    assert guard_run.returncode == 0
    save(output / 'summary.json', dict(passed=True, cache_format_version=7,
        origin_authenticated_replay_qualified=True, bounded_child_stdio_pipes=True,
        bounded_pipe_helper_sha256=digest(_PIPE_HELPER), completed=len(rows), planned=planned, rows=rows,
        product_sha256=digest(config['product']), actual_dependency_pins=dependencies,
        source_identities=qualification['source_identities'], configuration_sha256=digest(args.config),
        code_sha256=digest(__file__), code_pins=code_pins, fixtures_manifest_sha256=manifest_hash,
        build_receipts=[dict(path=str(qualification_path), sha256=qualification_hash)],
        scope=config['scope'], tiered_execution_qualified=False, hardware_efficiency_qualified=False))


if __name__ == '__main__':
    try:
        main()
    finally:
        __cache_source_context__.close()

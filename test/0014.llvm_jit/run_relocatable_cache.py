#!/usr/bin/env python3
"""Run a pinned real CLI in fresh processes; require signed persistent-cache hits."""

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
import os
from pathlib import Path
import re
import resource
import shlex
import subprocess
import time
_pipe_source = Path(__file__).with_name('cache_cli_process.py')
assert hashlib.sha256(_pipe_source.read_bytes()).hexdigest() == '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7', 'review changed bounded PIPE Source before import'
_captured_local_module = __cache_source_context__.load('cache_cli_process')
BoundedCli = _captured_local_module.BoundedCli
require_cache_phase = _captured_local_module.require_cache_phase


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify(pins):
    for path, expected in pins.items():
        assert digest(path) == expected, ('input changed', path)


def save(path, value):
    temp = path.with_name(path.name + '.tmp')
    temp.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')
    temp.replace(path)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('config', type=Path)
    a = p.parse_args()
    config = json.loads(a.config.read_text())

    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    verify(config['pins'])
    inputs = json.loads(Path(config['build_inputs']).read_text())
    verify(inputs['pins'])
    plan = json.loads(Path(config['build_plan']).read_text())
    receipts = []
    product = Path(config['product'])
    for index in config['build_indices']:
        label, argv = plan[index]
        path = Path(config['build_evidence']) / f'{index:03d}-{label}' / 'receipts.json'
        rows = json.loads(path.read_text())
        assert len(rows) == 1 and rows[0]['passed'] and rows[0]['returncode'] == 0
        assert rows[0]['argv'] == argv
        receipts.append({'path': str(path), 'sha256': digest(path), 'label': label})
    dependencies = {}
    for file in product.parent.glob('*.d'):
        paths = shlex.split(file.read_text().replace('\\\n', ' ').split(':', 1)[1])
        for path in paths:
            dependencies[path] = digest(path)
            if path in inputs['pins']:
                assert dependencies[path] == inputs['pins'][path], path
    assert dependencies
    pinned_product = {str(product): digest(product)}
    out = Path(config['output'])
    out.mkdir(exist_ok=False)
    runner = BoundedCli(config['execution_guard'], out, Path(__file__).resolve().parents[2])
    guard_source = Path(config['cgroup_guard']).resolve(strict=True)
    assert config['pins'].get(str(guard_source)) == digest(guard_source), 'pin actual cgroup guard Source before execution'
    guard_run = runner.execute('cgroup-guard-entry', ['/usr/bin/bash', str(guard_source)], timeout=30)
    assert guard_run.returncode == 0
    manifest = json.loads(Path(config['fixtures']).read_text())
    selected = set(config['cases'])
    assert selected and config['profiles'], 'empty positive replay plans cannot PASS'
    fixtures = [f for f in manifest['fixtures'] if f['name'] in selected]
    assert {f['name'] for f in fixtures} == selected
    fixture_pins = {f['path']: f['sha256'] for f in fixtures}
    verify(fixture_pins)
    rows = []
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')
    for profile in config['profiles']:
        # Actual v7 admission currently excludes lazy/tiered/DBG/preload Source.
        # An unsupported profile is a configuration failure, never a skipped PASS.
        argv = profile['argv']
        assert '-Raot' in argv or ('-Rcc' in argv and 'jit' in argv and '-Rcm' in argv and 'full' in argv)
        assert not any(x in argv for x in ('lazy','tiered','-Rdbg','-Wpre','-Rllvm-cache-no-sign','-Rllvm-cache-no-verify'))
        for fixture in fixtures:
            for path_mode in ('default', 'explicit'):
                name = profile['name'] + '--' + fixture['name'] + '--' + path_mode
                work = out / name
                work.mkdir()
                env = dict(os.environ)
                env.pop('UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT', None)
                env['XDG_CACHE_HOME'] = str(work / 'xdg')
                prefix = list(config.get('prefix', []))
                guest_root = None
                if prefix:
                    # QEMU can resolve root-based directory FDs under -L,
                    # including cache creation. Keep those writes outside the
                    # qualified loader/sysroot, and inspect their physical path.
                    loader_root = Path(prefix[prefix.index('-L') + 1])
                    guest_root = work / 'guest-root'
                    guest_root.mkdir()
                    for directory in ('lib', 'lib64', 'usr'):
                        target = loader_root / directory
                        if target.exists():
                            (guest_root / directory).symlink_to(target, target_is_directory=True)
                    prefix[prefix.index('-L') + 1] = str(guest_root)
                    prefix += ['-U', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT',
                               '-E', 'XDG_CACHE_HOME=' + env['XDG_CACHE_HOME']]
                namespace = config.get('cache_product', 'uwvm2')
                assert namespace in ('uwvm2', 'uwvm2ros')
                cache = work / ('xdg/' + namespace + '/llvm-jit' if path_mode == 'default' else 'cache')
                cache_flags = [] if path_mode == 'default' else [
                    '-Rllvm-cache-path', 'path', str(cache)]
                before = None
                for phase in ('cold', 'warm', 'warm-again'):
                    compiler_log = work / (phase + '.compiler.log')
                    run_log = work / (phase + '.run.log')
                    argv = [*prefix, str(product), *profile['argv'], '-Rct', '0',
                            *cache_flags, '-Rllvm-cache-seal-process', '-Rclog', 'err',
                            *fixture['argv'], '--run', fixture['path']]
                    started = time.monotonic()
                    completed = runner.execute(name + '--' + phase, argv, cwd=work, env=env,
                                               timeout=config.get('timeout', 120))
                    # Both child descriptors were pipes. Only the retired child's
                    # captured bytes are now persisted by this parent process.
                    compiler_log.write_bytes(completed.stderr)
                    run_log.write_bytes(completed.stdout + completed.stderr)
                    require_cache_phase(completed.stderr, phase)
                    actual_compiler_log = compiler_log
                    if not actual_compiler_log.exists() and guest_root is not None:
                        actual_compiler_log = guest_root / str(compiler_log).lstrip('/')
                    text = ansi.sub('', actual_compiler_log.read_text() if actual_compiler_log.exists() else '')
                    hit_lines = [line for line in text.splitlines() if 'object-cache-hit module=' in line]
                    physical_roots = [cache]
                    if guest_root is not None:
                        physical_roots.append(guest_root / str(cache).lstrip('/'))
                    entries = {}
                    for physical in physical_roots:
                        for f in physical.rglob('*.uwvm-ljc'):
                            key = str(f.relative_to(physical))
                            assert key not in entries, ('duplicate physical cache', key)
                            entries[key] = digest(f)
                    row = dict(profile=profile['name'], fixture=fixture['name'],
                               path_mode=path_mode, phase=phase, argv=argv,
                               exit=completed.returncode, seconds=time.monotonic() - started,
                               compiler_log_sha256=digest(actual_compiler_log) if actual_compiler_log.exists() else None,
                               physical_compiler_log=str(actual_compiler_log),
                               run_log_sha256=digest(run_log), cache_entries=entries,
                               physical_cache_roots=[str(p) for p in physical_roots],
                               signed_cache_hits=len(hit_lines), lifecycle_receipt=completed.lifecycle,
                               cache_format_version=7, actual_cli_process_sealed=True)
                    rows.append(row)
                    save(out / 'checkpoint.json', dict(state='running', rows=rows))
                    assert completed.returncode == 0, (name, phase, run_log.read_text()[-3000:])
                    assert entries, (name, phase, text[-4000:])
                    if phase == 'cold':
                        assert not hit_lines, (name, text)
                        assert re.search(r'object-cache-store-complete .* status=ok\b', text), (name, text[-4000:])
                        before = entries
                    else:
                        assert hit_lines and all('signature_verified=1' in line for line in hit_lines), (name, text[-4000:])
                        assert entries == before, (name, 'cache objects changed on hit')
                    row['passed'] = True
                    save(out / 'checkpoint.json', dict(state='running', rows=rows))
                print('PASS signed cold/warm/warm-again', name, flush=True)
    assert len(rows) == len(config['profiles']) * len(fixtures) * 6 and rows
    verify(config['pins'])
    verify(inputs['pins'])
    verify(dependencies)
    verify(pinned_product)
    verify(fixture_pins)
    guard_run = runner.execute('cgroup-guard-exit', ['/usr/bin/bash', str(guard_source)], timeout=30)
    assert guard_run.returncode == 0
    save(out / 'summary.json', dict(passed=True, rows=rows, completed=len(rows),
         cache_format_version=7, origin_authenticated_replay_qualified=True,
         bounded_child_stdio_pipes=True, execution_guard=config['execution_guard'],
         product_sha256=pinned_product[str(product)], source_identities=inputs['source_identities'],
         build_receipts=receipts, actual_dependency_pins=dependencies,
         configuration_sha256=digest(a.config), code_sha256=digest(__file__),
         bounded_pipe_helper_sha256=digest(Path(__file__).with_name('cache_cli_process.py')),
         scope='Real CLI in fresh processes, signed cold/store and repeated cache hits, default and explicit paths; only the listed target/profiles/features'))
    print('PASS real signed persistent cache', len(rows), flush=True)


if __name__ == '__main__':
    try:
        main()
    finally:
        __cache_source_context__.close()

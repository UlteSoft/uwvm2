#!/usr/bin/env python3
"""Require independent native unwinding of new Core 3 syntax, including cache replay.

Run the actual full-JIT CLI, optionally through a QEMU wrapper. Auto must select
native unwind on the tested target; an instruction fallback is a test failure.
Each cold compilation checks the emitted-frame policy, and fresh processes must
recover repeated recursive activations from the authenticated cached object.
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
import re
import resource
import subprocess
_pipe_source = Path(__file__).with_name('cache_cli_process.py')
assert hashlib.sha256(_pipe_source.read_bytes()).hexdigest() == '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7', 'review changed bounded PIPE Source before import'
_captured_local_module = __cache_source_context__.load('cache_cli_process')
BoundedCli = _captured_local_module.BoundedCli
require_cache_phase = _captured_local_module.require_cache_phase
accepts_expected_guest_trap = _captured_local_module.accepts_expected_guest_trap


CASES = {
    'multi-memory-scalar': ('', 'i32.const 65535 i32.load $small drop', []),
    'multi-memory-simd': ('', 'i32.const 65530 v128.load $small drop', []),
    'extended-const-memory': (
        '(global $offset i32 (i32.add (i32.const 65530) (i32.const 5)))',
        'global.get $offset i32.load $small drop', ['extended-const']),
    'relaxed-simd-memory': (
        '', '''i32.const 65530
        v128.const f32x4 1 1 1 1 v128.const f32x4 2 2 2 2 v128.const f32x4 3 3 3 3
        f32x4.relaxed_madd f32x4.extract_lane 0 i32.trunc_f32_s i32.add i32.load $small drop''',
        ['relaxed-simd']),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--wat2wasm', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--execution-guard', type=Path, required=True)
    parser.add_argument('--optimization', action='append', choices=('debug', 'pb-o3'))
    parser.add_argument('--cache', choices=('signed', 'disabled'), default='signed',
                        help='Use fresh compilation on targets with process-local address lowering; never count a miss as a replay')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]

    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.output.mkdir(parents=True, exist_ok=False)
    runner = BoundedCli(json.loads(args.execution_guard.read_text()), args.output, root)
    guard_source = root / 'tools/ci/require_wasm3_test_cgroup.sh'
    assert hashlib.sha256(guard_source.read_bytes()).hexdigest() == 'c8052568bd577a629b23742b15e86551a52552832c616822057f9d5b58c3c959', 'review changed guard Source before execution'
    guard_run = runner.execute('cgroup-guard-entry', ['/usr/bin/bash', str(guard_source)], timeout=30)
    assert guard_run.returncode == 0
    rows = []
    ansi = re.compile(r'\x1b\[[0-9;?]*[ -/]*[@-~]')
    for case, (declaration, body, extra_features) in CASES.items():
        wat = args.output / (case + '.wat')
        wasm = wat.with_suffix('.wasm')
        wat.write_text('''(module (memory $unused 2) (memory $small 1)
          %s
          (func $recurse (param $n i32)
            local.get $n if
              local.get $n i32.const 1 i32.sub call $recurse return
            end %s)
          (func (export "_start") (param $trap i32)
            local.get $trap if i32.const 7 call $recurse end))
''' % (declaration, body))
        features = ['multi-memory', *extra_features]
        produced = runner.execute(case + '--actual-producer', [str(args.wat2wasm), *['--enable-' + f for f in features],
                        str(wat), '-o', str(wasm)], timeout=60)
        assert produced.returncode == 0
        for optimization in args.optimization or ('debug', 'pb-o3'):
            for policy in ('instruction', 'unwind', 'auto'):
                label = case + '-' + optimization + '-' + policy
                common = [str(args.uwvm.resolve())]
                common += ['-Raot']
                common += ['-Rct', '0', '-Rllvm-full-policy', optimization,
                           '-Rllvm-call-stack', policy, '-Rllvm-cache-path']
                common += (['path', str((args.output / (label + '-cache')).resolve())]
                           if args.cache == 'signed' else ['disable'])
                if args.cache == 'signed':
                    common += ['-Rllvm-cache-seal-process']
                common += ['-WFE-' + feature for feature in features]
                for phase, argument in (('cold', '0'), ('replay1', '1'), ('replay2', '1')):
                    compile_log = args.output / (label + '-' + phase + '.compile.log')
                    command = common + ['-Rclog', 'err',
                                        '-Wstart', '1', argument, '--run', str(wasm.resolve())]
                    run = runner.execute(label + '--' + phase, command, timeout=120,
                        expected_termination='wasm-fatal-signal' if phase != 'cold' else None)
                    compile_log.write_bytes(run.stderr)
                    if args.cache == 'signed':
                        require_cache_phase(run.stderr, phase)
                    output = ansi.sub('', (run.stdout + run.stderr).decode(errors='replace'))
                    (args.output / (label + '-' + phase + '.log')).write_text(output)
                    compilation = compile_log.read_text() if compile_log.is_file() else ''
                    fields = dict(re.findall(r'\b(call_stack|call_stack_frames|unwind_check|unwind_replace_frames)=([^\s,;]+)', compilation))
                    stack = [int(n) for n in re.findall(r'\bfunc_idx=(\d+)', output)]
                    native = policy != 'instruction'
                    expected_policy = 'unwind' if native else 'instruction'
                    verified_hit = any('object-cache-hit ' in line and 'signature_verified=1' in line
                                       for line in compilation.splitlines())
                    generated_policy_ok = fields.get('call_stack') == expected_policy
                    generated_policy_ok &= fields.get('call_stack_frames') == ('omit' if native else 'emit')
                    if native:
                        generated_policy_ok &= fields.get('unwind_check') == 'live' and fields.get('unwind_replace_frames') == 'yes'
                    if phase == 'cold':
                        passed = run.returncode == 0 and generated_policy_ok
                    else:
                        passed = (accepts_expected_guest_trap(run.returncode, output, 'memory access out of bounds')
                                  and stack == [0] * 8 + [1])
                        passed &= verified_hit if args.cache == 'signed' else generated_policy_ok and not verified_hit
                    rows.append(dict(case=case, optimization=optimization, requested=policy, phase=phase,
                                     command=command, exit=run.returncode, fields=fields, stack=stack,
                                     cache_mode=args.cache, signature_verified_cache_hit=verified_hit, passed=bool(passed),
                                     cache_format_version=7, lifecycle_receipt=run.lifecycle))
                    (args.output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
                    if not passed:
                        raise RuntimeError(f'{label}/{phase}: native/frame/cache contract failed; see {args.output}')
                print(label, 'PASS generated policy and two recursive traps; cache=' + args.cache, flush=True)
    (args.output / 'metadata.json').write_text(json.dumps(dict(
        scope=__doc__, executable=str(args.uwvm.resolve()),
        executable_sha256=hashlib.sha256(args.uwvm.read_bytes()).hexdigest(),
        new_syntax_cases=list(CASES), cache_mode=args.cache, runs=len(rows),
        cache_format_version=7, bounded_child_stdio_pipes=True,
        persistent_cache_hit_qualified=args.cache == 'signed' and all(r['passed'] and r['signature_verified_cache_hit'] for r in rows if r['phase'] != 'cold'),
        hardware_efficiency_qualified=False), indent=2) + '\n')


if __name__ == '__main__':
    try:
        main()
    finally:
        __cache_source_context__.close()

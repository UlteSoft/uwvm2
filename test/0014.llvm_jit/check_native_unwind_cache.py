#!/usr/bin/env python3
"""Require exact recursive trap stacks after authenticated full-JIT cache replay.

This complements the in-memory RuntimeDyld regression: a real CLI must verify
the persistent object's signature, register its CFI again in a fresh process,
and recover repeated activations without generated logical frames in unwind
mode. Instruction tracking is checked independently, with its own cache key.
Requires the actual sealed CLI/self-image/Runtime origin and signing-enabled build and a working native unwind
provider. Unsupported configurations fail this required-capability test rather
than silently counting an instruction fallback or a cache miss as coverage.
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
import os
from pathlib import Path
import re
import resource
import signal
import subprocess
_pipe_source = Path(__file__).with_name('cache_cli_process.py')
assert hashlib.sha256(_pipe_source.read_bytes()).hexdigest() == '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7', 'review changed bounded PIPE Source before import'
_captured_local_module = __cache_source_context__.load('cache_cli_process')
BoundedCli = _captured_local_module.BoundedCli
require_cache_phase = _captured_local_module.require_cache_phase
accepts_expected_guest_trap = _captured_local_module.accepts_expected_guest_trap

TRAPS = {
    'unreachable': ('unreachable', 'catch unreachable'),
    'oob': ('i32.const -1 i64.load drop', 'memory access out of bounds'),
    'float-to-int': ('f32.const nan i32.trunc_f32_s drop', 'invalid conversion to integer'),
}
POLICIES = ('auto', 'debug', 'legacy-light', 'pb-o1', 'pb-o2', 'pb-o3')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('uwvm', type=Path)
    parser.add_argument('--out', type=Path, required=True, help='must be a new directory')
    parser.add_argument('--wat2wasm', default='wat2wasm')
    parser.add_argument('--execution-guard', type=Path, required=True)
    parser.add_argument('--policies', nargs='+', choices=POLICIES, default=list(POLICIES))
    parser.add_argument('--traps', nargs='+', choices=tuple(TRAPS), default=list(TRAPS))
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    binary = args.uwvm.resolve(strict=True)
    args.out.mkdir(parents=True, exist_ok=False)
    out = args.out.resolve()
    runner = BoundedCli(json.loads(args.execution_guard.read_text()), out, Path(__file__).resolve().parents[2])
    identity = hashlib.sha256(binary.read_bytes()).hexdigest()
    rows = []

    def invoke(command, log, expect_trap=False):
        process = runner.execute(log.stem, command, timeout=60,
            expected_termination='wasm-fatal-signal' if expect_trap else None)
        log.write_bytes(process.stdout + process.stderr)
        plain = re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', log.read_text(errors='replace'))
        return process.returncode, plain, process

    for trap in args.traps:
        body, diagnostic = TRAPS[trap]
        wat = out/(trap+'.wat')
        wasm = out/(trap+'.wasm')
        # Warm the SAME entry and module with argument 0, then trap with 1.
        # A trapping cold run can terminate before the async cache writer drains;
        # using such a run to seed the cache makes replay tests nondeterministic.
        # CLI start arguments are runtime data, not constants folded into this IR.
        wat.write_text(f'''(module (memory 1)
          (func $recurse (param $n i32)
            local.get $n if
              local.get $n i32.const 1 i32.sub call $recurse return
            end {body})
          (func (export "_start") (param $trap i32)
            local.get $trap if i32.const 7 call $recurse end))\n''')
        produced = runner.execute(trap + '--actual-wat-producer', [args.wat2wasm, str(wat), '-o', str(wasm)], timeout=60)
        assert produced.returncode == 0
        for policy in args.policies:
            for tracking in ('instruction', 'unwind'):
                label = f'{trap}-{policy}-{tracking}'
                cache = out/(label+'-cache')
                common = [str(binary), '-Rllvm-cache-seal-process', '-Raot', '-Rct', '0', '-Rclog', 'out', '-Rllvm-full-policy', policy,
                          '-Rllvm-call-stack', tracking, '-Rllvm-cache-path', 'path', str(cache)]
                for phase, argument in (('warm', '0'), ('replay1', '1'), ('replay2', '1')):
                    command = common + ['-Wstart', '1', argument, '--run', str(wasm)]
                    code, text, observed = invoke(command, out/(label+'-'+phase+'.log'), expect_trap=phase != 'warm')
                    require_cache_phase(observed.stderr + observed.stdout, 'cold' if phase == 'warm' else phase)
                    fields = dict(re.findall(r'\b(call_stack|call_stack_frames|unwind_check|unwind_replace_frames)=([^\s,;]+)', text))
                    frames = [int(n) for n in re.findall(r'\bfunc_idx=(\d+)', text)]
                    hit = any('object-cache-hit ' in line and 'signature_verified=1' in line for line in text.splitlines())
                    policy_ok = fields.get('call_stack') == tracking and fields.get('call_stack_frames') == ('emit' if tracking == 'instruction' else 'omit')
                    if tracking == 'unwind':
                        policy_ok &= fields.get('unwind_check') == 'live' and fields.get('unwind_replace_frames') == 'yes'
                    passed = policy_ok and (code == 0 if phase == 'warm' else
                        accepts_expected_guest_trap(code, text, diagnostic) and frames == [0]*8+[1] and hit)
                    row = {'trap': trap, 'policy': policy, 'tracking': tracking, 'phase': phase,
                           'command': command, 'returncode': code, 'frames': frames, 'signed_cache_hit': hit,
                           'fields': fields, 'passed': bool(passed), 'binary_sha256': identity,
                           'cache_format_version': 7, 'lifecycle_receipt': observed.lifecycle,
                           'origin_authenticated_replay_qualified': phase != 'warm' and bool(passed)}
                    rows.append(row)
                    (out/'results.json').write_text(json.dumps(rows, indent=2)+'\n')
                    if not passed:
                        raise RuntimeError(f'{label}/{phase} failed; see {out}')
    print(f'PASS {len(rows)} CLI runs; {sum(r["phase"] != "warm" for r in rows)} signed-cache recursive trap replays')


if __name__ == '__main__':
    try:
        main()
    finally:
        __cache_source_context__.close()

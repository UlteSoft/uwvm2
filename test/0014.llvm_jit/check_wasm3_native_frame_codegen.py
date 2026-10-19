#!/usr/bin/env python3
"""Inspect actual full-JIT objects for a new indexed-memory leaf on x86-64/AArch64/RV64.

Objects come from this run's private cache, not an offline substitute emitter.
The bounded decoder is inspection-only: it does not authenticate or execute
cache payloads. Signed-cache execution is checked by run_wasm3_native_unwind.py.
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
import struct
import subprocess
import sys

CACHE_FORMAT_VERSION = 7


def decode_object(blob, ros):
    if len(blob) < 64:
        raise ValueError('truncated cache header')
    magic, version, header, codec, signature, expected, size, isa, context, sig_size = struct.unpack('<8sIIIIQQQQQ', blob[:64])
    if magic != (b'UWVMROS\1' if ros else b'UWVMLJC\1') or version != CACHE_FORMAT_VERSION or header != 64 or signature != 1 or sig_size != 64:
        raise ValueError('unexpected cache format/signature layout')
    start = header + isa + context + sig_size
    if expected > 1048576 or start + size != len(blob):
        raise ValueError('invalid object size')
    payload = blob[start:]
    if codec == 0:
        result = payload
    elif codec == 2:
        result = bytearray()
        cursor = 0

        def byte():
            nonlocal cursor
            if cursor == len(payload):
                raise ValueError('truncated sequence')
            value = payload[cursor]
            cursor += 1
            return value

        def extended(length):
            while True:
                extra = byte()
                length += extra
                if length > expected:
                    raise ValueError('oversized sequence')
                if extra != 255:
                    return length

        while len(result) != expected:
            token = byte()
            literals = token >> 4
            if literals == 15:
                literals = extended(literals)
            if cursor + literals > len(payload) or len(result) + literals > expected:
                raise ValueError('invalid literal extent')
            result.extend(payload[cursor:cursor + literals])
            cursor += literals
            if len(result) == expected:
                break
            distance = byte() | (byte() << 8)
            count = 4 + (token & 15)
            if token & 15 == 15:
                count = extended(count)
            if not distance or distance > len(result) or len(result) + count > expected:
                raise ValueError('invalid backward match')
            for _ in range(count):
                result.append(result[-distance])
        if cursor != len(payload):
            raise ValueError('trailing payload')
    else:
        raise ValueError('unsupported inspection codec')
    if len(result) != expected or result[:4] != b'\x7fELF':
        raise ValueError('not the expected ELF object')
    return result


def frame_target(object_bytes):
    # This explicit inspection supports the same three little-endian ELF64 ISAs.
    # Select from the actual object, never from an empty tool-output header alone.
    if len(object_bytes) < 20 or object_bytes[:7] != b'\x7fELF\x02\x01\x01':
        raise ValueError('unsupported native-frame ELF class, endian or version')
    machine = struct.unpack_from('<H', object_bytes, 18)[0]
    targets = {
        62: ('x86_64', 'elf64-x86-64', r'\bcallq?\s'),
        183: ('aarch64', 'elf64-littleaarch64', r'\bbl[r]?\s'),
        243: ('riscv64', 'elf64-littleriscv', r'\b(?:jal|jalr)\s+ra\s*,|\bc\.jalr\s'),
    }
    if machine not in targets:
        raise ValueError('unsupported object ISA in this explicit assembly assertion')
    return targets[machine]


def require_frame_instruction_evidence(assembly, start, size):
    # The real caller requests the complete ELF symbol extent without raw bytes.
    # Local labels do not truncate that extent; relocations never count as code.
    if start < 0 or size <= 0:
        raise ValueError('invalid selected function extent')
    addresses, instructions = [], []
    for line in assembly.splitlines():
        match = re.match(r'^\s*([0-9a-fA-F]+):\s+(.*)$', line)
        if not match:
            continue
        tail = match[2].strip()
        if tail.startswith('R_'):
            continue
        opcode = re.match(r'([A-Za-z][A-Za-z0-9_.]*)\b', tail)
        if not opcode:
            raise ValueError('selected function contains an undecoded instruction row')
        address = int(match[1], 16)
        if not start <= address < start + size:
            raise ValueError('instruction is outside the selected ELF function')
        if addresses and address <= addresses[-1]:
            raise ValueError('selected instruction addresses are not strictly increasing')
        addresses.append(address)
        instructions.append(line)
    if not instructions or addresses[0] != start:
        raise ValueError('selected function has no instruction evidence at its start')
    return dict(instruction_lines=len(instructions), first_instruction=addresses[0],
                last_instruction=addresses[-1], relocation_lines_excluded=True,
                code='\n'.join(instructions))


def check_frame_instruction_evidence_contract():
    # Pure future-Linux classifier cases, never substitute Guest/tool receipts.
    for raw in ('', 'file format elf64-x86-64\n',
                '0000000000000000 <uwvm_m_abc_func_0>:\n',
                '  0: R_X86_64_64 callq\n', '  1: retq\n',
                '  0: retq\n  4: retq\n', '  0: retq\n  0: retq\n',
                '  0: retq\n  1: <unknown>\n'):
        try:
            require_frame_instruction_evidence(raw, 0, 4)
        except ValueError:
            pass
        else:
            raise AssertionError('empty, partial or invalid instruction evidence qualified')
    evidence = require_frame_instruction_evidence('  0: retq\n  0: R_X86_64_64 callq\n', 0, 1)
    assert evidence['instruction_lines'] == 1 and 'R_X86_64_64' not in evidence['code']


def main():
    _pipe_source = Path(__file__).with_name('cache_cli_process.py')
    assert hashlib.sha256(_pipe_source.read_bytes()).hexdigest() == '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7', 'review changed bounded PIPE Source before import'
    _captured_local_module = __cache_source_context__.load('cache_cli_process')
    BoundedCli = _captured_local_module.BoundedCli
    require_cache_phase = _captured_local_module.require_cache_phase
    function_extent = __cache_source_context__.load('elf_function_extent').function_extent
    check_frame_instruction_evidence_contract()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--llvm', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--execution-guard', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]

    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    guard = json.loads(args.execution_guard.read_text())
    runner = BoundedCli(guard, args.output, root)
    guard_source = root / 'tools/ci/require_wasm3_test_cgroup.sh'
    assert hashlib.sha256(guard_source.read_bytes()).hexdigest() == 'c8052568bd577a629b23742b15e86551a52552832c616822057f9d5b58c3c959', 'review changed guard Source before execution'
    guard_run = runner.execute('cgroup-guard-entry', ['/usr/bin/bash', str(guard_source)], timeout=30)
    assert guard_run.returncode == 0
    fixtures = args.output / 'fixtures'
    generated = runner.execute('actual-fixture-producer', __cache_source_context__.python_source_argv(root / 'test/0013.uwvm_int/wasm3/multi_memory_cases.py', [fixtures]))
    assert generated.returncode == 0
    rows = []
    for policy in ('instruction', 'unwind'):
        cache = args.output / (policy + '-cache')
        cache.mkdir(mode=0o700)
        command = [str(args.uwvm.resolve())]
        command += ['-Raot']
        command += ['-Rllvm-cache-seal-process', '-Rct', '0', '-Rllvm-call-stack', policy, '-Rllvm-full-policy', 'pb-o3',
                    '-Rllvm-cache-path', 'path', cache.name, '-Rclog', 'err',
                    '-WFE-multi-memory', '--run', str(fixtures / 'load-memory1-exec.wasm')]
        phases = []
        before = None
        for phase in ('cold', 'warm', 'warm-again'):
            run = runner.execute(policy + '--' + phase, command, cwd=args.output, timeout=120)
            (args.output / (policy + '--' + phase + '.stdout')).write_bytes(run.stdout)
            (args.output / (policy + '--' + phase + '.stderr')).write_bytes(run.stderr)
            if run.returncode:
                raise RuntimeError(f'{policy}: actual target execution failed')
            hits = require_cache_phase(run.stderr, phase)
            entries = {str(path.relative_to(cache)): hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in cache.rglob('*.uwvm-ljc')}
            assert len(entries) == 1, (policy, phase, entries)
            if before is None:
                before = entries
            else:
                assert entries == before, 'cache payload changed on real verified hit'
            phases.append(dict(phase=phase, signature_verified_hits=hits, lifecycle=run.lifecycle,
                               cache_entries=entries))
        files = list(cache.rglob('*.uwvm-ljc'))
        if len(files) != 1:
            raise RuntimeError(f'{policy}: expected one freshly emitted object, got {len(files)}')
        obj = args.output / (policy + '.o')
        object_bytes = decode_object(files[0].read_bytes(), args.ros)
        obj.write_bytes(object_bytes)
        arch, object_format, call_pattern = frame_target(object_bytes)
        tool = runner.execute(policy + '--llvm-symbols', [str(args.llvm / 'llvm-nm'), '--defined-only', str(obj)], timeout=60)
        assert tool.returncode == 0
        symbols = tool.stdout.decode('utf-8', errors='strict')
        leaf = re.findall(r'\b(uwvm_m_[0-9a-f]+_func_0)$', symbols, re.M)
        if len(leaf) != 1:
            raise RuntimeError('missing actual translated memory leaf')
        section, start, size = function_extent(object_bytes, leaf[0])
        # Select the complete real ELF symbol extent, including local labels.
        command_disassembly = [str(args.llvm / 'llvm-objdump'), '-dr', '--no-show-raw-insn',
                               '--section=' + section, '--start-address=' + str(start),
                               '--stop-address=' + str(start + size)]
        if arch == 'riscv64':
            # Distinguish link-setting calls from returns/linkless transfers.
            command_disassembly += ['-M', 'no-aliases']
        command_disassembly.append(str(obj))
        tool = runner.execute(policy + '--llvm-disassembly', command_disassembly, timeout=60)
        assert tool.returncode == 0
        dis = tool.stdout.decode('utf-8', errors='strict')
        (args.output / (policy + '.assembly.txt')).write_text(dis)
        if 'file format ' + object_format not in dis:
            raise RuntimeError('disassembly format differs from the actual ELF machine')
        evidence = require_frame_instruction_evidence(dis, start, size)
        calls = len(re.findall(call_pattern, evidence['code']))
        expected_calls = 2 if policy == 'instruction' else 0
        if calls != expected_calls:
            raise RuntimeError(f'{policy}: expected {expected_calls} frame-maintenance calls, got {calls}')
        tool = runner.execute(policy + '--llvm-sections', [str(args.llvm / 'llvm-objdump'), '-h', str(obj)], timeout=60)
        assert tool.returncode == 0
        sections = tool.stdout.decode('utf-8', errors='strict')
        (args.output / (policy + '.sections.txt')).write_text(sections)
        if policy == 'unwind' and '.eh_frame' not in sections:
            raise RuntimeError('native unwind object has no registered CFI section')
        evidence.pop('code')
        rows.append(dict(policy=policy, leaf=leaf[0], calls=calls,
                         section=section, start=start, size=size, architecture=arch,
                         disassembly_extent='bounded-ELF-function', instruction_evidence=evidence,
                         unwind_cfi='.eh_frame' in sections,
                         object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(), command=command, phases=phases,
                         cache_format_version=7, origin_authenticated_replay_qualified=True))
    assert len(rows) == 2 and {row['policy'] for row in rows} == {'instruction', 'unwind'}
    assert all(row['instruction_evidence']['instruction_lines'] > 0 for row in rows)
    (args.output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
    print('PASS actual new-syntax JIT objects: instruction has 2 frame calls; unwind has 0 and retains CFI')


if __name__ == '__main__':
    try:
        main()
    finally:
        __cache_source_context__.close()

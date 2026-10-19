#!/usr/bin/env python3
"""Inspect actual private-cache objects emitted and executed by the target VM.

The bounded existing decoder is inspection-only; it grants no cache-execution
authority. Normal VM execution handles authentication. Counts are evidence to
review, not an assertion of hardware throughput.
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
import importlib.util
import json
from pathlib import Path
import re
import stat
import subprocess
_captured_local_module = __cache_source_context__.load('run_matrix')
digest = _captured_local_module.digest
save = _captured_local_module.save
verify = _captured_local_module.verify
execute = _captured_local_module.execute
_captured_local_module = __cache_source_context__.load('architecture_catalog')
LINUX_PROFILES = _captured_local_module.LINUX_PROFILES
_captured_local_module = __cache_source_context__.load('elf_function_extent')
function_extent = _captured_local_module.function_extent


def instruction_evidence(assembly, arch):
    profile_arches = {row['id']: row['llvm_arch'] for row in LINUX_PROFILES}
    llvm_arch = profile_arches.get(arch, arch)
    groups = {
        'x86': 'i686', 'aarch64_be': 'aarch64',
        'armeb': 'arm', 'thumb': 'arm', 'thumbeb': 'arm',
        'riscv32': 'riscv64', 'riscv32be': 'riscv64', 'riscv64be': 'riscv64',
        'ppc': 'ppc64', 'ppcle': 'ppc64', 'ppc64le': 'ppc64',
        'mips': 'mips64', 'mipsel': 'mips64', 'mips64el': 'mips64',
        'sparcv9': 'sparc64', 'sparc': 'sparc64', 'sparcel': 'sparc64',
        'loongarch32': 'loongarch64', 'systemz': 's390x',
    }
    group = groups.get(llvm_arch, llvm_arch)
    rules = {
        'x86_64': (r'\bjmpq?\s+\*', r'\bcallq?\s'),
        'i686': (r'\bjmp[lq]?\s+\*', r'\bcall[lq]?\s'),
        'aarch64': (r'\bbr\s+x(?!30\b)\d+', r'\bbl(?:r)?\s'),
        'arm': (r'\bbx\s+(?:r(?:[0-9]|1[0-2])|ip)\b|\bldr\s+pc\s*,\s*\[(?!sp\b|r13\b)', r'\bblx?\s'),
        'riscv64': (r'\bjalr\s+zero\s*,\s*(?:0x0|0)\((?!ra\b)\w+\)|\bc\.jr\s+(?!ra\b)\w+',
                    r'\b(?:jal|jalr)\s+ra\s*,|\bc\.jalr\s'),
        'ppc64': (r'\bbctr\b', r'\b(?:bl|bctrl)\b'),
        'mips64': (r'\b(?:jr|jrc|jic)\s+\$?(?:t9|25)\b|\bjalr\s+\$?(?:zero|0)\s*,\s*\$?(?:t9|25)\b',
                   r'\b(?:jal|bal|balc|jialc)\b|\bjalr\s+(?!\$?(?:zero|0)\s*,)'),
        'sparc64': (r'\bjmp\s+%(?:[gl][0-7]|[oi][0-6])|\bjmpl\s+%(?:[gl][0-7]|[oi][0-6])[^\n]*,\s*%g0\b',
                    r'\bcall\b|\bjmpl\s+[^\n]*,\s*%o7\b'),
        # LLVM prints the zero-link JIRL as JR; exclude both RA spellings.
        'loongarch64': (r'\bjirl\s+\$?(?:zero|r0)\s*,\s*\$?(?!ra\b|r1\b)\w+|\bjr\s+\$?(?!ra\b|r1\b)\w+',
                        r'\bbl\b|\bjirl\s+\$?(?:ra|r1)\b'),
        's390x': (r'\bbr\s+%r(?!14\b)\d+\b', r'\b(?:brasl|basr|bras)\s+%r14\b'),
        'hexagon': (r'\bjumpr\s+r(?!31\b)\d+\b', r'\b(?:call|callr)\s'),
        'm68k': (r'\bjmp\s+', r'\b(?:jsr|bsr)\s'),
    }
    jump, call = rules[group]
    instructions = []
    for line in assembly.splitlines():
        match = re.match(r'^\s*[0-9a-f]+:\s+([A-Za-z][A-Za-z0-9_.]*)\b', line)
        if match and not match[1].startswith('R_'):
            instructions.append(line)
    code = '\n'.join(instructions)
    direct_tail_jumps = 0
    if group == 'aarch64':
        opcodes = {int(m[1], 16): m[2] for m in re.finditer(
            r'^\s*([0-9a-f]+):\s+([A-Za-z][A-Za-z0-9_.]*)', code, re.M)}
        for match in re.finditer(
                r'^\s*([0-9a-f]+):\s+R_AARCH64_JUMP26\s+(uwvm_m_[0-9a-f]+_func_\d+)\s*$', assembly, re.M):
            assert opcodes[int(match[1], 16)] == 'b'
            direct_tail_jumps += 1
    fp_pattern = r'\b(?:v?(?:add|sub|mul|div|sqrt|round)(?:ss|sd|ps|pd)|f(?:add|sub|mul|div|sqrt|rint)[.a-z]*|frint\w*|vfmul\w*)\b'
    if group == 'mips64':
        fp_pattern = r'\b(?:add|sub|mul|div|sqrt)\.[sd]\b|\b(?:ceil|floor|round|trunc)\.[wl]\.[sd]\b'
    return dict(direct_wasm_tail_jumps=direct_tail_jumps,
                indirect_linkless_jumps=len(re.findall(jump, code)),
                link_setting_calls=len(re.findall(call, code)),
                instruction_lines=len(instructions),
                architecture_group=group,
                relocation_lines_excluded=True,
                fp_instruction_lines=[line for line in instructions if re.search(fp_pattern, line)])



def require_instruction_evidence(assembly, arch):
    counts = instruction_evidence(assembly, arch)
    assert counts['instruction_lines'] > 0, 'selected function has no instruction evidence'
    return counts


def check_instruction_evidence_contract():
    # Deterministic future-Linux negative contract: successful objdump status
    # cannot turn empty/header/relocation-only output into selected instructions.
    for raw in ('', 'file format elf64-x86-64\n', '  0: R_X86_64_64 symbol\n'):
        try:
            require_instruction_evidence(raw, 'x86_64')
        except AssertionError:
            pass
        else:
            raise AssertionError('noninstruction output qualified')
    assert require_instruction_evidence('  0: retq\n', 'x86_64')['instruction_lines'] == 1

def main():
    import hashlib
    import sys
    helper = Path(__file__).resolve().parents[1] / '0014.llvm_jit/cache_cli_process.py'
    assert hashlib.sha256(helper.read_bytes()).hexdigest() == '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7', 'review changed bounded PIPE Source'
    _captured_local_module = __cache_source_context__.load('cache_cli_process')
    BoundedCli = _captured_local_module.BoundedCli
    require_cache_phase = _captured_local_module.require_cache_phase
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    parser.add_argument('output', type=Path)
    a = parser.parse_args()
    config = json.loads(a.config.read_text())
    check_instruction_evidence_contract()

    verify(config['pins'])
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    runner = BoundedCli(config['execution_guard'], out, Path(__file__).resolve().parents[2])
    guard_source = Path(config['cgroup_guard']).resolve(strict=True)
    assert config['pins'].get(str(guard_source)) == digest(guard_source), 'pin actual cgroup guard Source before execution'
    guard_run = runner.execute('cgroup-guard-entry', ['/usr/bin/bash', str(guard_source)], timeout=30)
    assert guard_run.returncode == 0
    decoder_path = Path(config['decoder']).resolve(strict=True)
    assert config['pins'].get(str(decoder_path)) == digest(decoder_path), 'pin the actual transitive v7 decoder Source'
    module = __cache_source_context__.load_path(decoder_path, config['pins'][str(decoder_path)])
    assert module.CACHE_FORMAT_VERSION == 7, 'legacy decoder cannot qualify v7 objects'
    fixtures = {f['name']: f for f in json.loads(Path(config['fixtures']).read_text())['fixtures']}
    rows = []
    assert config['cases'] and config.get('stack_policies', ['instruction', 'unwind']), 'empty inspection plans cannot PASS'
    for name in config['cases']:
        fixture = fixtures[name]
        assert digest(fixture['path']) == fixture['sha256']
        for level in (1, 2, 3):
            for policy in config.get('stack_policies', ['instruction', 'unwind']):
                label = f'{name}-O{level}-{policy}'
                work = out / label
                work.mkdir()
                cache = work / 'cache'
                cache.mkdir(mode=0o700)
                mode = ['-Raot'] # Actual stable/ROS CLI; ros only selects format/namespace.
                prefix = list(config.get('prefix', []))
                object_source = config.get('object_source', 'private-cache')
                obj = work / 'actual.o'
                if object_source == 'exclusive-test-capture':
                    # Only separately qualified test-macro products may emit
                    # these bytes. The ordinary product/cache policy is kept.
                    assert config['test_capture_macro_qualified'] is True
                    assert prefix and Path(prefix[0]).name.startswith('qemu-')
                    prefix += ['-U', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT',
                               '-E', 'UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT=' + str(obj)]
                    cache_flags = ['-Rllvm-cache-path', 'disable']
                else:
                    assert object_source == 'private-cache', object_source
                    cache_flags = ['-Rllvm-cache-seal-process', '-Rllvm-cache-path', 'path', cache.name]
                argv = [*prefix, config['product'], *mode, '-Rct', '0',
                        '-Rllvm-full-policy', f'pb-o{level}', '-Rllvm-call-stack', policy,
                        *cache_flags, *(['-Rclog', 'err'] if object_source == 'private-cache' else ['-Rclog', 'file', 'compiler.log']),
                        *fixture['argv'], '--run', fixture['path']]
                if object_source == 'exclusive-test-capture':
                    actual = runner.execute(label + '--exclusive-capture', argv, cwd=work, timeout=config.get('timeout', 120))
                    (work / 'run.log').write_bytes(actual.stdout + actual.stderr)
                    row = dict(argv=argv, exit=actual.returncode, lifecycle_receipt=actual.lifecycle)
                    row['origin_authenticated_replay_qualified'] = False
                else:
                    phases, before = [], None
                    for phase in ('cold', 'warm', 'warm-again'):
                        actual = runner.execute(label + '--' + phase, argv, cwd=work, timeout=config.get('timeout', 120))
                        (work / (phase + '.stdout')).write_bytes(actual.stdout)
                        (work / (phase + '.compiler.log')).write_bytes(actual.stderr)
                        assert actual.returncode == 0
                        hits = require_cache_phase(actual.stderr, phase)
                        current = {str(path.relative_to(cache)): digest(path) for path in cache.rglob('*.uwvm-ljc')}
                        assert len(current) == 1
                        if before is None:
                            before = current
                        else:
                            assert current == before, 'executed cache changed on verified fresh-process hit'
                        phases.append(dict(phase=phase, signature_verified_hits=hits,
                                           cache_entries=current, lifecycle_receipt=actual.lifecycle))
                    row = dict(argv=argv, exit=actual.returncode, phases=phases, cache_format_version=7,
                               origin_authenticated_replay_qualified=True)
                assert row['exit'] == 0, row
                entries = list(cache.rglob('*.uwvm-ljc'))
                if object_source == 'exclusive-test-capture':
                    assert not entries, (label, entries)
                    info = obj.lstat()
                    assert stat.S_ISREG(info.st_mode) and info.st_nlink == 1
                    assert stat.S_IMODE(info.st_mode) == 0o600
                    assert obj.read_bytes()[:4] == b'\x7fELF'
                    cache_hash = None
                else:
                    assert len(entries) == 1, (label, entries)
                    obj.write_bytes(module.decode_object(entries[0].read_bytes(), config['ros']))
                    cache_hash = digest(entries[0])
                tool = runner.execute(label + '--llvm-symbols', [config['llvm_nm'], '--defined-only', str(obj)], timeout=60)
                assert tool.returncode == 0
                symbols = tool.stdout.decode('utf-8', errors='strict')
                functions = [0, 1] if name.startswith('tail-') else [0]
                evidence = []
                for index in functions:
                    found = re.findall(r'\b(uwvm_m_[0-9a-f]+_func_' + str(index) + r')$', symbols, re.M)
                    assert len(found) == 1, (label, index, found)
                    direct_extent_arches = {'x86_64', 'i686', 'aarch64', 'aarch64_be',
                                            'riscv64', 'riscv32', 'riscv32be', 'riscv64be'}
                    if config['arch'] in direct_extent_arches:
                        section, start, size = function_extent(obj.read_bytes(), found[0])
                        command = [config['llvm_objdump'], '-dr', '--no-show-raw-insn',
                                   '--section=' + section, '--start-address=' + str(start),
                                   '--stop-address=' + str(start + size)]
                        disassembly_extent = 'bounded-ELF-function'
                    else:
                        # Descriptor/Thumb ABIs need their own entry resolution.
                        # Preserve their existing selector until separately qualified.
                        command = [config['llvm_objdump'], '-dr', '--no-show-raw-insn',
                                   '--disassemble-symbols=' + found[0]]
                        disassembly_extent = 'llvm-symbol-selector'
                    if config['arch'].startswith('riscv'):
                        command += ['-M', 'no-aliases']
                    tool = runner.execute(label + '--llvm-function-' + str(index), [*command, str(obj)], timeout=60)
                    assert tool.returncode == 0
                    assembly = tool.stdout.decode('utf-8', errors='strict')
                    asm = work / f'function-{index}.asm'
                    asm.write_text(assembly)
                    counts = require_instruction_evidence(assembly, config['arch'])
                    if name.startswith('tail-'):
                        assert counts['indirect_linkless_jumps'] + counts['direct_wasm_tail_jumps'] > 0, (label, index, assembly)
                    evidence.append(dict(function=found[0], disassembly_extent=disassembly_extent,
                                         assembly_sha256=digest(asm), **counts))
                assert evidence and len(evidence) == len(functions) and all(item['instruction_lines'] > 0 for item in evidence)
                row.update(case=name, optimization=level, stack_policy=policy,
                           object_sha256=digest(obj), cache_sha256=cache_hash, object_source=object_source,
                           functions=evidence,
                           cold_trap_call_paths_qualified=False, hardware_efficiency_qualified=False)
                rows.append(row)
                save(out / 'checkpoint.json', dict(state='running', rows=rows))
                print('PASS executed actual object', label, flush=True)
    assert rows and digest(helper) == '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7'
    verify(config['pins'])
    guard_run = runner.execute('cgroup-guard-exit', ['/usr/bin/bash', str(guard_source)], timeout=30)
    assert guard_run.returncode == 0
    save(out / 'summary.json', dict(passed=True, rows=rows, config_sha256=digest(a.config),
        product_sha256=digest(config['product']), scope=config['scope'],
        cache_format_version=7, bounded_pipe_helper_sha256=digest(helper),
        origin_authenticated_replay_qualified=bool(rows) and all(row['origin_authenticated_replay_qualified'] for row in rows),
        hardware_efficiency_qualified=False, cold_trap_call_paths_qualified=False))


if __name__ == '__main__':
    try:
        main()
    finally:
        __cache_source_context__.close()

#!/usr/bin/env python3
"""Inspect objects from completed, real signed-cache executions.

No object is loaded or executed by this reader. The VM's cold/warm/warm-again
receipts supply execution evidence. Each selected symbol is decoded over its
complete ELF function extent, including functions in separate lazy groups.
Instruction counts describe generated code, not hardware throughput.
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
import subprocess
import time

_captured_local_module = __cache_source_context__.load('elf_function_extent')
function_extent = _captured_local_module.function_extent
_captured_local_module = __cache_source_context__.load('inspect_jit')
instruction_evidence = _captured_local_module.instruction_evidence
_captured_local_module = __cache_source_context__.load('run_matrix')
digest = _captured_local_module.digest
save = _captured_local_module.save
verify = _captured_local_module.verify


def main():
    import hashlib
    import sys
    helper = Path(__file__).resolve().parents[1] / '0014.llvm_jit/cache_cli_process.py'
    assert hashlib.sha256(helper.read_bytes()).hexdigest() == '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7'
    _captured_local_module = __cache_source_context__.load('cache_cli_process')
    BoundedCli = _captured_local_module.BoundedCli
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    args = parser.parse_args()
    config = json.loads(args.config.read_text())

    verify(config['pins'])
    output = Path(config['output'])
    output.mkdir(exist_ok=False)
    runner = BoundedCli(config['execution_guard'], output, Path(__file__).resolve().parents[2])
    guard_source = Path(config['cgroup_guard']).resolve(strict=True)
    assert config['pins'].get(str(guard_source)) == digest(guard_source), 'pin actual cgroup guard Source before execution'
    guard_run = runner.execute('cgroup-guard-entry', ['/usr/bin/bash', str(guard_source)], timeout=30)
    assert guard_run.returncode == 0
    summary_path = Path(config['summary'])
    deadline = time.monotonic() + config.get('prerequisite_timeout', 3000)
    while not summary_path.is_file():
        state = json.loads(Path(config['execution_stage_status']).read_text())
        assert state['state'] != 'failed', state
        assert time.monotonic() < deadline, 'Waiting for signed-cache execution'
        time.sleep(10)
    summary_hash = digest(summary_path)
    summary = json.loads(summary_path.read_text())
    assert summary['passed'] and summary['completed'] == len(summary['rows'])
    assert summary['cache_format_version'] == 7 and summary['origin_authenticated_replay_qualified'], 'legacy execution receipts remain historical'
    tiered = config.get('execution_kind') == 'tiered-numeric'
    if tiered:
        build_path = Path(config['build_qualification'])
        product_qualification_hash = summary.get('fresh_build_qualification_sha256',
                                                summary.get('prior_product_qualification_sha256'))
        assert product_qualification_hash and digest(build_path) == product_qualification_hash
        build = json.loads(build_path.read_text())
        assert build['passed'] and build['product_sha256'] == summary['product_sha256']
        assert build['source_identities'] == summary['source_identities']
        assert build['actual_dependency_pins'] == summary['actual_dependency_pins']
    else:
        for receipt in summary['build_receipts']:
            assert digest(receipt['path']) == receipt['sha256'], receipt
    verify(summary['actual_dependency_pins'])
    matrix_path = Path(config['matrix_config'])
    assert digest(matrix_path) == summary['config_sha256' if tiered else 'configuration_sha256']
    matrix = json.loads(matrix_path.read_text())
    verify(matrix['pins'])
    assert digest(matrix['product']) == summary['product_sha256']
    if tiered:
        verify(summary['code_pins'])
        assert digest(config['execution_code']) == summary['code_pins'][config['execution_code']]
        available_profiles = [f'{mode}-O{level}' for mode in matrix['modes'] for level in matrix['levels']]
        tiered_case = ('tiered-memory64-high' if matrix.get('fixture_kind') == 'memory64-high'
                       else 'tiered-simd-sign' if matrix.get('fixture_kind') == 'simd-sign'
                       else 'tiered-simd-rounding' if matrix.get('fixture_kind') == 'simd-rounding'
                       else 'tiered-numeric')
        available_cases = [tiered_case]
    else:
        assert digest(config['execution_code']) == summary['code_sha256']
        available_profiles = [p['name'] for p in matrix['profiles']]
        available_cases = matrix['cases']
    decoder_path = Path(config['decoder']).resolve(strict=True)
    assert config['pins'].get(str(decoder_path)) == digest(decoder_path), 'pin the actual transitive v7 decoder Source'
    decoder = __cache_source_context__.load_path(decoder_path, config['pins'][str(decoder_path)])
    assert decoder.CACHE_FORMAT_VERSION == 7, 'legacy decoder cannot qualify v7 objects'
    profiles = config.get('profiles', available_profiles)
    cases = config['cases']
    assert profiles and cases and summary['rows'], 'empty requested/current execution selection cannot qualify inspection'
    assert len(set(profiles)) == len(profiles) and len(set(cases)) == len(cases)
    assert set(profiles) <= set(available_profiles)
    assert set(cases) <= set(available_cases)
    qualified = {}
    for row in summary['rows']:
        assert row['passed'] and row['exit'] == 0
        if tiered:
            assert row['actual_t2_target_entry'] and row['full_module_ready']
            assert row['counters']['tiered_full_ready'] > 0 and row['counters']['tiered_full_failed'] == 0
            expected_checks = (12 if matrix.get('fixture_kind') == 'simd-sign' else
                               24 if matrix.get('fixture_kind') == 'simd-rounding' else 6)
            assert row['checked_calls'] == 4_000_000 and row['bit_checks_per_call'] == expected_checks
            # Preserve the original summary bytes; names here only select rows.
            row = {**row, 'profile': f"{row['mode']}-O{row['optimization']}", 'fixture': tiered_case}
        key = (row['profile'], row['fixture'], row['path_mode'], row['phase'])
        assert key not in qualified, key
        qualified[key] = row
    rows = []
    for profile in profiles:
        for case in cases:
            phases = [qualified[(profile, case, 'default', phase)]
                      for phase in ('cold', 'warm', 'warm-again')]
            cold, warm, original = phases
            assert cold['signed_cache_hits'] == 0
            assert warm['signed_cache_hits'] > 0 and original['signed_cache_hits'] > 0
            assert cold['cache_entries'] == warm['cache_entries'] == original['cache_entries']
            assert original['cache_entries']
            for phase in phases:
                log = Path(phase['physical_compiler_log'])
                assert digest(log) == phase['compiler_log_sha256'], log
                if phase['phase'] != 'cold':
                    hit_lines = [line for line in log.read_text().splitlines()
                                 if 'object-cache-hit module=' in line]
                    assert len(hit_lines) == phase['signed_cache_hits']
                    assert all('signature_verified=1' in line for line in hit_lines)
            work = output / (profile + '--' + case)
            work.mkdir()
            objects = []
            for entry_index, (key, expected) in enumerate(sorted(original['cache_entries'].items())):
                relative = Path(key)
                assert not relative.is_absolute() and '..' not in relative.parts
                found = [Path(root) / key for root in original['physical_cache_roots']
                         if (Path(root) / key).is_file()]
                assert len(found) == 1 and digest(found[0]) == expected, (key, found)
                obj = work / (str(entry_index) + '-executed-cache-object.o')
                obj.write_bytes(decoder.decode_object(found[0].read_bytes(), config['ros']))
                tool = runner.execute(profile + '--' + case + '--llvm-symbols-' + str(entry_index), [config['llvm_nm'], '--defined-only', str(obj)], timeout=60)
                assert tool.returncode == 0
                symbols = tool.stdout.decode('utf-8', errors='strict')
                tool = runner.execute(profile + '--' + case + '--llvm-relocations-' + str(entry_index), [config['llvm_readobj'], '--relocations', str(obj)], timeout=60)
                assert tool.returncode == 0
                relocations = tool.stdout.decode('utf-8', errors='strict')
                reloc = work / (str(entry_index) + '-relocations.txt')
                reloc.write_text(relocations)
                objects.append(dict(path=str(obj), symbols=symbols,
                                    cache_path=str(found[0]), cache_sha256=expected,
                                    object_sha256=digest(obj), relocations_sha256=digest(reloc)))
            functions = []
            indices = config.get('function_indices', {}).get(case, [0, 1] if case.startswith('tail-') else [0])
            assert indices and all(type(i) is int and i >= 0 for i in indices) and len(set(indices)) == len(indices), 'nonempty exact function selection required'
            selected_objects = objects
            if tiered:
                # T1 and T2 deliberately reuse function symbol names in
                # different MCJIT engines. Select the full three-function T2
                # object and its loop-reentry entry before choosing the hot
                # core; a lazy T1 hot-function object is not T2 evidence.
                selected_objects = [obj for obj in objects if all(re.search(
                    r'\buwvm_m_[0-9a-f]+_func_' + str(fn) + r'$', obj['symbols'], re.M)
                    for fn in (0, 1, 2)) and re.search(
                    r'\buwvm_m_[0-9a-f]+_tiered_loop_raw_func_2_off_\d+$', obj['symbols'], re.M)]
                assert len(selected_objects) == 1, (profile, 'Missing unique full T2 object')
            for index in indices:
                stem = '_tiered_core_func_' if tiered else '_func_'
                candidates = [(obj, name) for obj in selected_objects for name in re.findall(
                    r'\b(uwvm_m_[0-9a-f]+' + stem + str(index) + r')$', obj['symbols'], re.M)]
                assert len(candidates) == 1, (profile, case, index, candidates)
                obj, symbol = candidates[0]
                section, start, size = function_extent(Path(obj['path']).read_bytes(), symbol)
                argv = [config['llvm_objdump'], '-dr', '--no-show-raw-insn',
                        '--section=' + section, '--start-address=' + str(start),
                        '--stop-address=' + str(start + size)]
                if config['arch'].startswith('riscv'):
                    argv += ['-M', 'no-aliases']
                tool = runner.execute(profile + '--' + case + '--llvm-function-' + str(index), [*argv, obj['path']], timeout=60)
                assert tool.returncode == 0
                assembly = tool.stdout.decode('utf-8', errors='strict')
                asm = work / ('function-' + str(index) + '.asm')
                asm.write_text(assembly)
                counts = instruction_evidence(assembly, config['arch'])
                assert counts['instruction_lines'] > 0, (case, symbol)
                if case.startswith('tail-'):
                    assert counts['indirect_linkless_jumps'] + counts['direct_wasm_tail_jumps'] > 0, (profile, case, index, assembly)
                functions.append(dict(function=symbol, section=section, start=start, size=size,
                                      disassembly_extent='bounded-ELF-function',
                                      object_sha256=obj['object_sha256'], assembly_sha256=digest(asm), **counts))
            for obj in objects:
                assert digest(obj['cache_path']) == obj['cache_sha256']
            assert objects and functions and all(f['instruction_lines'] > 0 for f in functions)
            rows.append(dict(profile=profile, fixture=case,
                             objects=[{k: v for k, v in obj.items() if k != 'symbols'} for obj in objects],
                             functions=functions, signed_execution_summary_sha256=summary_hash))
            save(output / 'checkpoint.json', dict(state='running', rows=rows))
            print('PASS actual authenticated cache assembly', config['arch'], profile, case, flush=True)
    assert rows and len(rows) == len(profiles) * len(cases)
    verify(config['pins'])
    verify(matrix['pins'])
    if tiered:
        verify(summary['code_pins'])
        assert digest(build_path) == product_qualification_hash
    verify(summary['actual_dependency_pins'])
    assert digest(summary_path) == summary_hash
    guard_run = runner.execute('cgroup-guard-exit', ['/usr/bin/bash', str(guard_source)], timeout=30)
    assert guard_run.returncode == 0
    save(output / 'summary.json', dict(passed=True, rows=rows, completed=len(rows),
         source_identities=summary['source_identities'], product_sha256=summary['product_sha256'],
         configuration_sha256=digest(args.config), code_sha256=digest(__file__),
         signed_execution_summary_sha256=summary_hash,
         hardware_efficiency_qualified=False,
         scope='Actual signed cache execution in fresh processes; complete selected function extents across full and lazy groups. Instruction observations only.'))


if __name__ == '__main__':
    try:
        main()
    finally:
        __cache_source_context__.close()

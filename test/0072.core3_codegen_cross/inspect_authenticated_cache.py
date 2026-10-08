#!/usr/bin/env python3
"""Inspect objects from completed, real signed-cache executions.

No object is loaded or executed by this reader. The VM's cold/warm/warm-again
receipts supply execution evidence. Each selected symbol is decoded over its
complete ELF function extent, including functions in separate lazy groups.
Instruction counts describe generated code, not hardware throughput.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import time

from elf_function_extent import function_extent
from inspect_jit import instruction_evidence
from run_matrix import digest, save, verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    args = parser.parse_args()
    config = json.loads(args.config.read_text())
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    verify(config['pins'])
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
                       else 'tiered-numeric')
        available_cases = [tiered_case]
    else:
        assert digest(config['execution_code']) == summary['code_sha256']
        available_profiles = [p['name'] for p in matrix['profiles']]
        available_cases = matrix['cases']
    spec = importlib.util.spec_from_file_location('inspection_only_decoder', config['decoder'])
    decoder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(decoder)
    output = Path(config['output'])
    output.mkdir(exist_ok=False)
    profiles = config.get('profiles', available_profiles)
    cases = config['cases']
    assert set(profiles) <= set(available_profiles)
    assert set(cases) <= set(available_cases)
    qualified = {}
    for row in summary['rows']:
        assert row['passed'] and row['exit'] == 0
        if tiered:
            assert row['actual_t2_target_entry'] and row['full_module_ready']
            assert row['counters']['tiered_full_ready'] > 0 and row['counters']['tiered_full_failed'] == 0
            expected_checks = 12 if matrix.get('fixture_kind') == 'simd-sign' else 6
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
                symbols = subprocess.check_output([config['llvm_nm'], '--defined-only', str(obj)], text=True)
                relocations = subprocess.check_output([config['llvm_readobj'], '--relocations', str(obj)], text=True)
                reloc = work / (str(entry_index) + '-relocations.txt')
                reloc.write_text(relocations)
                objects.append(dict(path=str(obj), symbols=symbols,
                                    cache_path=str(found[0]), cache_sha256=expected,
                                    object_sha256=digest(obj), relocations_sha256=digest(reloc)))
            functions = []
            indices = config.get('function_indices', {}).get(case, [0, 1] if case.startswith('tail-') else [0])
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
                assembly = subprocess.check_output([*argv, obj['path']], text=True)
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
            rows.append(dict(profile=profile, fixture=case,
                             objects=[{k: v for k, v in obj.items() if k != 'symbols'} for obj in objects],
                             functions=functions, signed_execution_summary_sha256=summary_hash))
            save(output / 'checkpoint.json', dict(state='running', rows=rows))
            print('PASS actual authenticated cache assembly', config['arch'], profile, case, flush=True)
    assert len(rows) == len(profiles) * len(cases)
    verify(config['pins'])
    verify(matrix['pins'])
    if tiered:
        verify(summary['code_pins'])
        assert digest(build_path) == product_qualification_hash
    verify(summary['actual_dependency_pins'])
    assert digest(summary_path) == summary_hash
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    save(output / 'summary.json', dict(passed=True, rows=rows, completed=len(rows),
         source_identities=summary['source_identities'], product_sha256=summary['product_sha256'],
         configuration_sha256=digest(args.config), code_sha256=digest(__file__),
         signed_execution_summary_sha256=summary_hash,
         hardware_efficiency_qualified=False,
         scope='Actual signed cache execution in fresh processes; complete selected function extents across full and lazy groups. Instruction observations only.'))


if __name__ == '__main__':
    main()

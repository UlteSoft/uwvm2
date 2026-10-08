#!/usr/bin/env python3
"""Stage Wasmtime-checked Core 3 GC/EH/thread inputs for ordinary Windows LLVM-full."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

from prepare_windows_debug_fixtures import check_cgroup


GC_STEMS = ('i31_roundtrip', 'i31_bulk', 'struct_roundtrip',
            'array_roundtrip', 'i31_element', 'i31_into_anyref_table',
            'gc_struct_into_anyref_table', 'i31_table64')
BRANCH_STEMS = ('branch_semantics_br_if', 'branch_semantics_br_on_null',
                'branch_semantics_br_on_non_null')
GAP_STEMS = ('ref-test-exn-null', 'ref-cast-exn-null', 'br-on-cast-exn-null',
             'exn-cast-retained', 'ref-cast-exn-null-trap', 'exnref_call_retained',
             'eh-cross-function-catch-ref', 'eh-cross-function-win64-seh')
GAP_ABI_CASES = {'exnref_call_retained', 'eh-cross-function-catch-ref',
                 'eh-cross-function-win64-seh'}
GAP_TRAPS = {'ref-cast-exn-null-trap'}
EXNREF_WAT_SHA256 = '2ec5cd90f0759f69af2087fc44110e18fc60f6723f81a10ea99d7e83f2e8c0bf'
EXNREF_WASM_SHA256 = '1cdc8e931df2783033c91ee557bdaad487fae9a744e8b207f40994af30cfc727'
INITIALIZER_WAT_SHA256 = '0859c6aec8db4cf71a7f15d3d31501a91546a22c34078acd40bf1337c8a71dbb'
INITIALIZER_WASM_SHA256 = '42a7d935a710b00789466f09ca1252edf000af4714dac28e4d2a792754df118a'


def sha256(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(value: bool, message: str) -> None:
    if not value:
        raise ValueError(message)


def reference_run(command: list[str], log: Path) -> dict[str, object]:
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            check=False)
    log.write_bytes(result.stdout)
    require(result.returncode == 0, f'reference command failed: {command!r}; {log}')
    return {'command': command, 'exit_code': result.returncode,
            'log_sha256': sha256(log)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'build', 'gc-evidence', 'eh-evidence',
                 'gc-const-evidence', 'exnref-evidence', 'initializer-evidence',
                 'wasm-tools', 'wasmtime', 'gap-dir', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--docker-scope', required=True)
    args = parser.parse_args()
    cgroup = check_cgroup(args.docker_scope)
    source = args.source_root.resolve(strict=True)
    build = args.build.resolve(strict=True)
    gc = args.gc_evidence.resolve(strict=True)
    eh = args.eh_evidence.resolve(strict=True)
    gc_const = args.gc_const_evidence.resolve(strict=True)
    exnref = args.exnref_evidence.resolve(strict=True)
    initializer = args.initializer_evidence.resolve(strict=True)
    wasm_tools = args.wasm_tools.resolve(strict=True)
    wasmtime = args.wasmtime.resolve(strict=True)
    gap = args.gap_dir.resolve(strict=True)
    output = args.output.resolve()
    require(output.is_relative_to(Path('/tmp')) or output.is_relative_to(Path('/dev/shm')),
            'VM staging must reside on tmpfs')
    output.mkdir(mode=0o700, parents=True, exist_ok=False)

    built = json.loads((build / 'build.json').read_text())
    require(built.get('status') == 'cross-built-awaiting-real-windows-vm',
            'ordinary Windows LLVM-full PE has not linked')
    require({key: built.get('build_cgroup', {}).get(key) for key in cgroup} == cgroup,
            'ordinary Windows PE was built outside the staging cgroup or CPU set')
    current = subprocess.check_output(
        [sys.executable, str(source / 'tools/ci/wasm3_source_fingerprint.py'),
         str(source), str(output / 'source-current.json')], text=True).strip()
    require(current == built['source_id'], 'ordinary Windows PE source ID changed')
    for relative, expected in built['build_recipe_files_sha256'].items():
        require(sha256(source / relative) == expected,
                f'ordinary Windows build recipe changed: {relative}')
    product = Path(built['products']['uwvm.exe']['path']).resolve(strict=True)
    require(sha256(product) == built['products']['uwvm.exe']['sha256'],
            'ordinary Windows PE changed after linking')

    gc_summary = json.loads((gc / 'summary.json').read_text())
    gc_runs = json.loads((gc / 'runs.json').read_text())
    require(gc_summary.get('passed') is True and gc_summary.get('checks', 0) >= 59,
            'GC/table64 reference evidence lacks its 59 passing checks')
    require(gc_summary.get('wasm_tools_sha256') == sha256(wasm_tools) and
            gc_summary.get('wasmtime_sha256') == sha256(wasmtime),
            'GC/table64 reference tools changed')
    passed_cases = {record.get('case') for record in gc_runs
                    if record.get('passed') is True and record.get('exit') == 0}
    for stem in GC_STEMS:
        require({stem + '-parse', stem + '-validate', stem + '-wasmtime'} <= passed_cases,
                f'GC/table64 reference lacks parse, validate or Wasmtime pass: {stem}')
    eh_summary = json.loads((eh / 'summary.json').read_text())
    eh_commands = json.loads((eh / 'commands.json').read_text())
    eh_wasm = eh / 'core3-eh-threads.wasm'
    require(eh_summary.get('passed') is True and
            eh_summary.get('wasm_sha256') == sha256(eh_wasm),
            'EH/shared-atomic reference evidence changed')
    require({'validate', 'wasmtime'} <= {record.get('label') for record in eh_commands
                                         if record.get('exit') == 0},
            'EH/shared-atomic reference lacks validation or Wasmtime pass')
    const_summary = json.loads((gc_const / 'summary.json').read_text())
    const_runs = json.loads((gc_const / 'runs.json').read_text())
    const_wasm = gc_const / 'gc_const_expr_execution.wasm'
    require(const_wasm.is_file() and not const_wasm.is_symlink(),
            'missing GC constant-expression fixture')
    require(const_summary.get('fixtures', {}).get('gc_const_expr_execution', {}).get(
                'wasm_sha256') == sha256(const_wasm),
            'GC constant-expression fixture differs from oracle evidence')
    require(const_summary.get('wasm_tools_sha256') == sha256(wasm_tools) and
            const_summary.get('wasmtime_sha256') == sha256(wasmtime),
            'GC constant-expression oracle tools changed')
    const_passes = {record.get('name') for record in const_runs
                    if record.get('phase') == 'oracle' and record.get('passed') is True
                    and record.get('exit') == 0}
    require({f'gc_const_expr_execution-{phase}'
             for phase in ('parse', 'validate', 'wasmtime')} <= const_passes,
            'GC constant-expression oracle parse, validation or Wasmtime pass missing')
    exnref_wat = source / 'test/0017.runtime/fixtures/exnref_struct_payload_execution.wat'
    exnref_wasm = exnref / 'exnref_struct_payload_execution.wasm'
    require(exnref_wat.is_file() and sha256(exnref_wat) == EXNREF_WAT_SHA256 and
            exnref_wasm.is_file() and not exnref_wasm.is_symlink() and
            sha256(exnref_wasm) == EXNREF_WASM_SHA256,
            'Core 3 exnref payload fixture or frozen source WAT changed')
    initializer_wat = source / 'test/0014.llvm_jit/fixtures/wasm3_initializers.wat'
    initializer_wasm = initializer / 'wasm3_initializers.wasm'
    require(initializer_wat.is_file() and
            sha256(initializer_wat) == INITIALIZER_WAT_SHA256 and
            initializer_wasm.is_file() and not initializer_wasm.is_symlink() and
            sha256(initializer_wasm) == INITIALIZER_WASM_SHA256,
            'Core 3 ref.func table initializer fixture or frozen WAT changed')

    gap_manifest_path = gap / 'manifest.json'
    gap_manifest = json.loads(gap_manifest_path.read_text())
    require(gap_manifest.get('schema') == 1 and
            gap_manifest.get('status') ==
            'wasm-tools-parse-validate-and-wasmtime48-outcomes-passed' and
            gap_manifest.get('wasm_tools_sha256') == sha256(wasm_tools) and
            gap_manifest.get('wasmtime_sha256') == sha256(wasmtime) and
            gap_manifest.get('cgroup_memory_max') == '68719476736' and
            gap_manifest.get('cgroup_swap_max') == '0',
            'Core 3 exception-reference cast oracle is not qualified')
    gap_rows = {row.get('stem'): row for row in gap_manifest.get('fixtures', [])}
    require(set(gap_rows) == set(GAP_STEMS),
            'Core 3 exception-reference cast fixture set changed')
    for stem in GAP_STEMS:
        row = gap_rows[stem]
        wat = source / 'test/0017.runtime/fixtures' / (stem + '.wat')
        wasm = gap / (stem + '.wasm')
        require(wat.is_file() and not wat.is_symlink() and
                sha256(wat) == row['wat_sha256'] and
                sha256(gap / (stem + '.wat')) == row['wat_sha256'] and
                wasm.is_file() and not wasm.is_symlink() and
                sha256(wasm) == row['wasm_sha256'] and
                len(row.get('checks', [])) == 3 and
                all(check.get('passed') is True for check in row['checks']) and
                all(sha256(gap / f'{stem}-step{i}.log') ==
                    check.get('log_sha256')
                    for i, check in enumerate(row['checks'])) and
                all((check.get('exit_code') != 0) ==
                    (stem in GAP_TRAPS and i == 2)
                    for i, check in enumerate(row['checks'])),
                f'Core 3 exception-reference cast oracle changed: {stem}')

    reference_runs: dict[str, dict[str, object]] = {}
    origins: dict[str, Path] = {'uwvm.exe': product,
                                'core3-eh-threads.wasm': eh_wasm,
                                'gc_const_expr_execution.wasm': const_wasm,
                                'exnref_struct_payload_execution.wasm': exnref_wasm,
                                'wasm3_initializers.wasm': initializer_wasm,
                                'run_core3_windows_full_vm.ps1':
                                    source / 'test/0017.runtime/run_core3_windows_full_vm.ps1'}
    for stem in GC_STEMS:
        wasm = gc / (stem + '.wasm')
        require(wasm.is_file() and not wasm.is_symlink(), f'missing GC fixture: {wasm}')
        reference_runs[stem + '-validate'] = reference_run(
            [str(wasm_tools), 'validate', str(wasm)], output / (stem + '-validate.log'))
        reference_runs[stem + '-wasmtime'] = reference_run(
            [str(wasmtime), 'run', '-C', 'cache=n', '-W', 'gc=y', str(wasm)],
            output / (stem + '-wasmtime.log'))
        origins[stem + '.wasm'] = wasm
    reference_runs['core3-eh-threads-validate'] = reference_run(
        [str(wasm_tools), 'validate', str(eh_wasm)],
        output / 'core3-eh-threads-validate.log')
    reference_runs['core3-eh-threads-wasmtime'] = reference_run(
        [str(wasmtime), 'run', '-C', 'cache=n', '-W', 'exceptions=y',
         '-W', 'threads=y', '-W', 'shared-memory=y', str(eh_wasm)],
        output / 'core3-eh-threads-wasmtime.log')
    reference_runs['gc_const_expr_execution-validate'] = reference_run(
        [str(wasm_tools), 'validate', '--features', 'all', str(const_wasm)],
        output / 'gc_const_expr_execution-validate.log')
    reference_runs['gc_const_expr_execution-wasmtime'] = reference_run(
        [str(wasmtime), 'run', '-C', 'cache=n', '-W', 'gc=y',
         '-W', 'extended-const=n', str(const_wasm)],
        output / 'gc_const_expr_execution-wasmtime.log')
    reproduced_exnref = output / 'exnref-parse-reproduced.wasm'
    reference_runs['exnref_struct_payload_execution-parse'] = reference_run(
        [str(wasm_tools), 'parse', str(exnref_wat), '-o', str(reproduced_exnref)],
        output / 'exnref_struct_payload_execution-parse.log')
    require(sha256(reproduced_exnref) == EXNREF_WASM_SHA256,
            'reparsing frozen exnref WAT differs from the independently tested Wasm')
    reproduced_exnref.unlink()
    reference_runs['exnref_struct_payload_execution-validate'] = reference_run(
        [str(wasm_tools), 'validate', '--features', 'all', str(exnref_wasm)],
        output / 'exnref_struct_payload_execution-validate.log')
    reference_runs['exnref_struct_payload_execution-wasmtime'] = reference_run(
        [str(wasmtime), 'run', '-C', 'cache=n', '-W', 'exceptions=y',
         '-W', 'gc=y', str(exnref_wasm)],
        output / 'exnref_struct_payload_execution-wasmtime.log')
    reproduced_initializer = output / 'initializer-parse-reproduced.wasm'
    reference_runs['wasm3_initializers-parse'] = reference_run(
        [str(wasm_tools), 'parse', str(initializer_wat), '-o',
         str(reproduced_initializer)], output / 'wasm3_initializers-parse.log')
    require(sha256(reproduced_initializer) == INITIALIZER_WASM_SHA256,
            'reparsing frozen table initializer WAT changed its Wasm bytes')
    reproduced_initializer.unlink()
    reference_runs['wasm3_initializers-validate'] = reference_run(
        [str(wasm_tools), 'validate', '--features', 'all', str(initializer_wasm)],
        output / 'wasm3_initializers-validate.log')
    # Wasmtime treats explicit table expression initializers as part of its
    # function-references switch; its standard oracle therefore uses default
    # proposal flags. UWVM's separate table-initializer gate is tested in VM.
    reference_runs['wasm3_initializers-wasmtime'] = reference_run(
        [str(wasmtime), 'run', '-C', 'cache=n', str(initializer_wasm)],
        output / 'wasm3_initializers-wasmtime.log')

    # Exercise both paths of the Core 3 typed-branch instructions in the real
    # Windows product. Rebuild each binary from this source snapshot, then
    # independently validate and execute the exact bytes staged for the VM.
    branch_wat_hashes: dict[str, str] = {}
    generated = output / 'generated'
    generated.mkdir(mode=0o700)
    for stem in BRANCH_STEMS:
        wat = source / 'test/0014.llvm_jit/fixtures' / (stem + '.wat')
        require(wat.is_file() and not wat.is_symlink(),
                f'missing frozen typed-branch source fixture: {wat}')
        branch_wat_hashes[stem] = sha256(wat)
        wasm = generated / (stem + '.wasm')
        reference_runs[stem + '-parse'] = reference_run(
            [str(wasm_tools), 'parse', str(wat), '-o', str(wasm)],
            output / (stem + '-parse.log'))
        reference_runs[stem + '-validate'] = reference_run(
            [str(wasm_tools), 'validate', '--features', 'all', str(wasm)],
            output / (stem + '-validate.log'))
        reference_runs[stem + '-wasmtime'] = reference_run(
            [str(wasmtime), 'run', '-C', 'cache=n', '-W',
             'function-references=y', str(wasm)],
            output / (stem + '-wasmtime.log'))
        origins[stem + '.wasm'] = wasm

    # Reparse the frozen source WAT and rerun both independent oracles before
    # transferring these four exact binaries to the real Windows guest.
    for stem in GAP_STEMS:
        wat = source / 'test/0017.runtime/fixtures' / (stem + '.wat')
        wasm = gap / (stem + '.wasm')
        reproduced = generated / (stem + '-reproduced.wasm')
        reference_runs[stem + '-parse'] = reference_run(
            [str(wasm_tools), 'parse', str(wat), '-o', str(reproduced)],
            output / (stem + '-parse.log'))
        require(sha256(reproduced) == sha256(wasm),
                f'Core 3 exception-reference cast bytes differ from source: {stem}')
        reproduced.unlink()
        reference_runs[stem + '-validate'] = reference_run(
            [str(wasm_tools), 'validate', str(wasm)],
            output / (stem + '-validate.log'))
        if stem not in GAP_TRAPS:
            reference_runs[stem + '-wasmtime'] = reference_run(
                [str(wasmtime), 'run', '-C', 'cache=n', '-W',
                 'gc=n' if stem in GAP_ABI_CASES else 'gc=y', '-W',
                 'exceptions=y', str(wasm)],
                output / (stem + '-wasmtime.log'))
        else:
            command = [str(wasmtime), 'run', '-C', 'cache=n', str(wasm)]
            process = subprocess.run(command, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, check=False)
            log = output / (stem + '-wasmtime.log')
            log.write_bytes(process.stdout)
            require(process.returncode != 0 and
                    b'wasm trap: cast failure' in process.stdout,
                    f'Core 3 ref.cast trap oracle changed: {stem}')
            reference_runs[stem + '-wasmtime'] = {
                'command': command, 'exit_code': process.returncode,
                'expected_success': False,
                'diagnostic': 'wasm trap: cast failure',
                'log_sha256': sha256(log)}
        origins[stem + '.wasm'] = wasm

    fixture_hashes = {name: sha256(path) for name, path in origins.items()
                      if name.endswith('.wasm')}
    qualification = {'source_id': current, 'product_sha256': sha256(product),
                     'llvm_certificate_sha256': built['llvm_certificate_sha256'],
                     'gc_reference_summary_sha256': sha256(gc / 'summary.json'),
                     'eh_reference_summary_sha256': sha256(eh / 'summary.json'),
                     'gc_const_reference_summary_sha256': sha256(gc_const / 'summary.json'),
                     'exnref_source_wat_sha256': sha256(exnref_wat),
                     'initializer_source_wat_sha256': sha256(initializer_wat),
                     'branch_source_wat_sha256': branch_wat_hashes,
                     'gap_oracle_sha256': sha256(gap_manifest_path),
                     'wasm_tools_sha256': sha256(wasm_tools),
                     'wasmtime_sha256': sha256(wasmtime),
                     'fixture_sha256': fixture_hashes,
                     'reference_runs': reference_runs, 'cgroup': cgroup}
    qualification_file = output / 'qualification.json'
    qualification_file.write_text(json.dumps(qualification, indent=2, sort_keys=True) + '\n')
    origins['qualification.json'] = qualification_file
    files: dict[str, str] = {}
    for name, path in origins.items():
        require(path.is_file() and not path.is_symlink(), f'missing regular staging input: {path}')
        if path != qualification_file:
            with path.open('rb') as original, (output / name).open('xb') as staged:
                shutil.copyfileobj(original, staged)
        files[name] = sha256(output / name)
        require(files[name] == sha256(path), f'staging changed input: {name}')
    after = subprocess.check_output(
        [sys.executable, str(source / 'tools/ci/wasm3_source_fingerprint.py'),
         str(source), str(output / 'source-after-staging.json')], text=True).strip()
    require(after == current, 'ordinary Windows source changed during Core 3 staging')
    result = {'schema': 1, 'source_id': current, 'files': files,
              'product_sha256': qualification['product_sha256'],
              'llvm_certificate_sha256': built['llvm_certificate_sha256'],
              'gap_oracle_sha256': sha256(gap_manifest_path),
              'reference_runs': reference_runs, 'cgroup': cgroup}
    (output / 'stage.json').write_text(json.dumps(result, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'source_id': current, 'output': str(output),
                      'fixtures': len(fixture_hashes), 'reference_checks': len(reference_runs)},
                     sort_keys=True))


if __name__ == '__main__':
    main()

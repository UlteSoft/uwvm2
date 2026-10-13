#!/usr/bin/env python3
"""Stage exact-byte ROS Core 3 Windows VM fixtures and their qualified PE."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil


GC_STEMS = ('i31_roundtrip', 'i31_bulk', 'struct_roundtrip',
            'array_roundtrip', 'i31_element', 'i31_into_anyref_table',
            'gc_struct_into_anyref_table', 'i31_table64')
BRANCH_STEMS = ('branch_semantics_br_if', 'branch_semantics_br_on_null',
                'branch_semantics_br_on_non_null')
GAP_STEMS = ('ref-test-exn-null', 'ref-cast-exn-null', 'br-on-cast-exn-null',
             'exn-cast-retained', 'ref-cast-exn-null-trap', 'exnref_call_retained',
             'eh-cross-function-catch-ref', 'eh-cross-function-win64-seh')
GAP_TRAPS = {'ref-cast-exn-null-trap'}
GC_CONST_SHA256 = 'a25485ef0b598f6f7206fe6b7ae8c29c103d850e0e25d7d42ff58bb2805c9086'
EXNREF_STRUCT_SHA256 = '1cdc8e931df2783033c91ee557bdaad487fae9a744e8b207f40994af30cfc727'
INITIALIZER_SHA256 = '42a7d935a710b00789466f09ca1252edf000af4714dac28e4d2a792754df118a'
WASM_TOOLS_SHA256 = '115d5986a8a1aeb112a5f2d98209c144f70c26188a5d41e266698a1e20de4ed1'
WASMTIME_SHA256 = '9f3f3e1b1b048f802c1d35a17607c064425a9c746f00bda3c2466b92ddcb5c92'


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'build', 'gc', 'gc-const', 'exnref', 'initializer', 'eh',
                 'typed', 'throw-ref', 'br-on-null', 'branch-dir', 'gap-dir', 'script', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    source = args.source_root.resolve(strict=True)
    build = json.loads((args.build / 'build.json').read_text())
    require(build['status'] == 'cross-built-awaiting-real-windows-vm',
            'ROS Windows LLVM-full product has not linked')
    guest_script = args.script.read_text()
    require('$result.source_id = [string]$qualification.source_id' in guest_script and
            'Require ($result.product_sha256 -ieq $qualification.product_sha256)' in guest_script,
            'guest Core 3 script must bind its source ID and PE to the qualification')
    product = Path(build['products']['uwvm.exe']['path'])
    require(digest(product).lower() == build['products']['uwvm.exe']['sha256'].lower(),
            'cross-built Windows PE changed before Core 3 staging')
    require(digest(args.gc_const) == GC_CONST_SHA256,
            'single-op GC constant fixture differs from Wasmtime-validated bytes')
    require(digest(args.exnref) == EXNREF_STRUCT_SHA256,
            'GC exception payload fixture differs from the Wasmtime-validated bytes')
    require(digest(args.initializer) == INITIALIZER_SHA256,
            'Core 3 ref.func table initializer differs from the Wasmtime-validated bytes')
    branch_summary_path = args.branch_dir / 'summary.json'
    branch_summary = json.loads(branch_summary_path.read_text())
    require(branch_summary.get('passed') is True and
            branch_summary.get('source_id') == build['source_id'] and
            {row.get('stem') for row in branch_summary.get('fixtures', [])} == set(BRANCH_STEMS),
            'Core 3 branch reference suite is missing or not source-matched')
    for row in branch_summary['fixtures']:
        require(digest(args.branch_dir / (row['stem'] + '.wasm')) == row['wasm_sha256'] and
                all(run.get('exit_code') == 0 for run in row.get('runs', [])) and
                len(row.get('runs', [])) == 3,
                f'Core 3 branch oracle changed: {row.get("stem")}')
    gap_manifest_path = args.gap_dir / 'manifest.json'
    gap_manifest = json.loads(gap_manifest_path.read_text())
    require(gap_manifest.get('schema') == 1 and
            gap_manifest.get('status') ==
            'wasm-tools-parse-validate-and-wasmtime48-outcomes-passed' and
            gap_manifest.get('wasm_tools_sha256') == WASM_TOOLS_SHA256 and
            gap_manifest.get('wasmtime_sha256') == WASMTIME_SHA256 and
            gap_manifest.get('cgroup_memory_max') == '68719476736' and
            gap_manifest.get('cgroup_swap_max') == '0',
            'Core 3 exception-reference cast oracle is not qualified')
    gap_rows = {row.get('stem'): row for row in gap_manifest.get('fixtures', [])}
    require(set(gap_rows) == set(GAP_STEMS),
            'Core 3 exception-reference cast fixture set changed')
    for stem in GAP_STEMS:
        row = gap_rows[stem]
        require(digest(args.gap_dir / (stem + '.wat')) == row['wat_sha256'] and
                digest(source / 'test/0017.runtime/fixtures' / (stem + '.wat')) ==
                row['wat_sha256'] and
                digest(args.gap_dir / (stem + '.wasm')) == row['wasm_sha256'] and
                len(row.get('checks', [])) == 3 and
                all(check.get('passed') is True for check in row['checks']) and
                all(digest(args.gap_dir / f'{stem}-step{i}.log') ==
                    check.get('log_sha256')
                    for i, check in enumerate(row['checks'])) and
                all((check.get('exit_code') != 0) ==
                    (stem in GAP_TRAPS and i == 2)
                    for i, check in enumerate(row['checks'])),
                f'Core 3 exception-reference cast oracle changed: {stem}')
    output = args.output.resolve()
    require(output.is_relative_to(Path('/tmp')) or
            output.is_relative_to(Path('/dev/shm')),
            'Windows VM staging must stay on temporary storage')
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    origins = {
        'uwvm.exe': product,
        'core3-eh-threads.wasm': args.eh,
        'core3-typed-ref.wasm': args.typed,
        'core3-throw-ref-bottom.wasm': args.throw_ref,
        'core3-br-on-null-bottom.wasm': args.br_on_null,
        'gc_const_expr_execution.wasm': args.gc_const,
        'exnref_struct_payload_execution.wasm': args.exnref,
        'wasm3_initializers.wasm': args.initializer,
        'run_core3_windows_vm.ps1': args.script,
    }
    origins.update({stem + '.wasm': args.gc / (stem + '.wasm')
                    for stem in GC_STEMS})
    origins.update({stem + '.wasm': args.branch_dir / (stem + '.wasm')
                    for stem in BRANCH_STEMS})
    origins.update({stem + '.wasm': args.gap_dir / (stem + '.wasm')
                    for stem in GAP_STEMS})
    qualification = {'schema': 1, 'source_id': build['source_id'],
                     'product_sha256': build['products']['uwvm.exe']['sha256'],
                     'llvm_certificate_sha256': build['llvm_certificate_sha256'],
                     'branch_reference_sha256': digest(branch_summary_path),
                     'gap_oracle_sha256': digest(gap_manifest_path),
                     'files_sha256': {name: digest(origin)
                                      for name, origin in origins.items()}}
    qualification_path = output / 'qualification.json'
    qualification_path.write_text(json.dumps(qualification, indent=2, sort_keys=True) + '\n')
    origins['qualification.json'] = qualification_path
    files: dict[str, str] = {}
    for name, origin in origins.items():
        require(origin.is_file() and not origin.is_symlink(),
                f'missing regular Windows Core 3 input: {origin}')
        target = output / name
        if target != origin:
            with origin.open('rb') as source, target.open('xb') as destination:
                shutil.copyfileobj(source, destination)
        files[name] = digest(target)
        require(files[name] == digest(origin), f'Core 3 copy changed: {name}')
        if name != 'qualification.json':
            require(files[name] == qualification['files_sha256'][name],
                    f'Core 3 input changed between qualification and staging: {name}')
    manifest = {'schema': 1, 'source_id': build['source_id'],
                'product_sha256': files['uwvm.exe'],
                'llvm_certificate_sha256': build['llvm_certificate_sha256'],
                'branch_reference_sha256': digest(branch_summary_path),
                'gap_oracle_sha256': digest(gap_manifest_path),
                'files': files}
    with (output / 'manifest.json').open('x') as stream:
        json.dump(manifest, stream, indent=2, sort_keys=True)
        stream.write('\n')
    print(json.dumps({'source_id': manifest['source_id'],
                      'files': len(files), 'output': str(output)}, sort_keys=True))


if __name__ == '__main__':
    main()

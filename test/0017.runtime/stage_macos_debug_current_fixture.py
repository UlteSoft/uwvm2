#!/usr/bin/env python3
"""Official C5 O1 fixture/oracle staging on keeper-controlled Linux only."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
import sys
import run_debug_source_step_cli as source_cli


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(value: bool, message: str) -> None:
    if not value:
        raise RuntimeError(message)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'out', 'clang', 'wasm-ld', 'wasm-tools', 'llvm-dwarfdump'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    require(sys.platform == 'linux', 'official staging is admitted Linux only')
    root = args.source_root.resolve(strict=True)
    require(Path(__file__).resolve() == root / 'test/0017.runtime/stage_macos_debug_current_fixture.py' and
            Path(source_cli.__file__).resolve() == root / 'test/0017.runtime/run_debug_source_step_cli.py' and
            Path(source_cli.metadata_cli.__file__).resolve() == root / 'test/0017.runtime/run_debug_source_inline_metadata_cli.py', 'actual source/oracle module paths mismatch')
    guard = root / 'tools/ci/require_wasm3_test_cgroup.sh'
    subprocess.run(['bash', str(guard)], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=False)
    source = root / 'test/0017.runtime/fixtures/debug_source_step_c.c'
    numeric_sources = {name: root / 'test/0017.runtime/fixtures' / filename for name, filename in (
        ('numeric', 'debug_current_macos_numeric.wat'), ('replace', 'debug_current_macos_replace.wat'), ('replacement', 'debug_current_macos_replace_new.wat'))}
    tools = {name: getattr(args, name).resolve(strict=True) for name in ('clang', 'wasm_ld', 'wasm_tools', 'llvm_dwarfdump')}
    inputs = {str(path): sha(path) for path in (source, guard, Path(__file__).resolve(), Path(source_cli.__file__).resolve(),
                                             Path(source_cli.metadata_cli.__file__).resolve(), *tools.values(), *numeric_sources.values())}
    receipt = {'schema': 1, 'purpose': 'actual-official-c5-O1-fixture-for-current-macos', 'passed': False,
               'source': str(source), 'source_sha256': sha(source), 'inputs_before': inputs, 'commands': []}

    def run(argv: list[str], name: str) -> Path:
        log = out / (name + '.log')
        row = {'argv': argv, 'cwd': str(root), 'tool_sha256': sha(Path(argv[0])), 'log': str(log), 'returncode': None}
        receipt['commands'].append(row)
        with log.open('xb') as stream:
            completed = subprocess.run(argv, cwd=root, stdout=stream, stderr=subprocess.STDOUT, timeout=180, check=False)
        row.update(returncode=completed.returncode, log_sha256=sha(log))
        require(completed.returncode == 0 and log.stat().st_size <= 16 << 20, 'actual official command failed or output exceeds budget')
        return log

    try:
        obj, wasm = out / 'c5-O1.o', out / 'c5-O1.wasm'
        run([str(tools['clang']), '--target=wasm32-unknown-unknown', '-g', '-gdwarf-5', '-O1', '-std=c17', '-nostdlib',
             '-c', str(source), '-o', str(obj)], 'compile')
        object_sha = sha(obj)
        run([str(tools['wasm_ld']), '--no-entry', '--export-all', str(obj), '-o', str(wasm)], 'link')
        require(sha(obj) == object_sha, 'actual object changed during official link')
        wasm_sha = sha(wasm)
        run([str(tools['wasm_tools']), 'validate', str(wasm)], 'validate')
        run([str(tools['llvm_dwarfdump']), '--verify', str(wasm)], 'verify')
        oracle = run([str(tools['llvm_dwarfdump']), '--debug-info', '--debug-line', '--debug-ranges', '--debug-rnglists', str(wasm)], 'oracle')
        text = oracle.read_text()
        require(text.count('DW_TAG_inlined_subroutine') >= 4 and 'source_step_inner' in text and 'source_step_middle' in text,
                'official C5 compiler output has no real nested/repeated inline metadata')
        expressions = source_cli.code_expressions(wasm)
        sequences = source_cli.line_sequences(text)
        markers = {}
        for marker in ('STEP_LEAF_ENTRY', 'STEP_PHYSICAL_CALL', 'STEP_AFTER_CALL'):
            rows = [number for number, line in enumerate(source.read_text().splitlines(), 1) if marker in line]
            require(len(rows) == 1, 'actual unique source marker missing')
            markers[marker] = rows[0]
        require(sha(wasm) == wasm_sha and sha(obj) == object_sha, 'official output changed during validation/oracle')
        receipt.update(object=str(obj), object_sha256=object_sha, wasm=str(wasm), wasm_sha256=wasm_sha,
                       oracle=str(oracle), oracle_sha256=sha(oracle), markers=markers,
                       expressions={str(key): value for key, value in expressions.items()},
                       actual_import_policy='all actual imports rejected', dwarf_version=5, optimization='O1',
                       official_statement_rows=sum(row['is_statement'] for sequence in sequences for row in sequence))
        receipt['numeric_modules'] = {}
        for name, wat in numeric_sources.items():
            module = out / (name + '.wasm')
            run([str(tools['wasm_tools']), 'parse', str(wat), '-o', str(module)], name + '-encode')
            module_sha = sha(module)
            run([str(tools['wasm_tools']), 'validate', str(module)], name + '-validate')
            require(sha(module) == module_sha and not any(kind in (2, 5, 11) for kind, _ in source_cli.metadata_cli.sections(module)), 'actual no-memory/no-import numeric module changed')
            source_cli.code_expressions(module)
            receipt['numeric_modules'][name] = {'wat': str(wat), 'wat_sha256': sha(wat), 'wasm': str(module), 'wasm_sha256': module_sha}
        subprocess.run(['bash', str(guard)], check=True)
        receipt['inputs_after'] = {path: sha(Path(path)) for path in inputs}
        require(receipt['inputs_after'] == inputs, 'actual stage source/tool inputs changed')
        receipt['passed'] = True
    except BaseException as error:
        receipt['error'] = repr(error)
        raise
    finally:
        (out / 'fixture.receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('PASS official C5 O1 fixture/oracle staging only; no macOS product execution')


if __name__ == '__main__':
    main()

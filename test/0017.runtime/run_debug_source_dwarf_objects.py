#!/usr/bin/env python3
"""Remote-only bounded object components and genuine C/C++/Rust DWARF5.

The keeper must independently build/pin the metadata executable with the fresh
candidate LLVM-DWARF closure and supply its SHA. This recipe does not qualify
that link closure, production runtime pause authority, or guest-memory reads.
All native invocations require the existing 64GiB/swap0 Linux test cgroup.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import resource
import shlex
import subprocess
import sys


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--cxx', type=Path, required=True)
    parser.add_argument('--cxxflag', action='append', default=[])
    parser.add_argument('--ldflag', action='append', default=[])
    parser.add_argument('--clang', type=Path, required=True)
    parser.add_argument('--wasm-ld', type=Path, required=True)
    parser.add_argument('--rustc', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--metadata-executable', type=Path, required=True)
    parser.add_argument('--metadata-executable-sha256', required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'linux':
        raise RuntimeError('actual native recipe is remote Linux only')
    root = args.source_root.resolve(strict=True)
    if Path(__file__).resolve(strict=True) != root / 'test/0017.runtime/run_debug_source_dwarf_objects.py':
        raise RuntimeError('runner must belong to the exact candidate source root')
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    tools = {name: getattr(args, name.replace('-', '_')).resolve(strict=True)
             for name in ('cxx', 'clang', 'wasm-ld', 'rustc', 'wasm-tools', 'metadata-executable')}
    tools_before = {name: sha(path) for name, path in tools.items()}
    if tools_before['metadata-executable'] != args.metadata_executable_sha256.lower():
        raise RuntimeError('keeper-pinned metadata executable bytes do not match')
    if any(value.startswith('@') for value in args.cxxflag + args.ldflag):
        raise RuntimeError('hidden response-file arguments are not source-pinned')
    sources = ['src/uwvm2/uwvm/debugger/' + name for name in (
        'source_dwarf_types.h', 'source_dwarf_types.cppm', 'source_dwarf_index.h', 'source_dwarf_index.cppm',
        'source_dwarf_values.h', 'source_dwarf_values.cppm', 'source_dwarf_query.h', 'source_dwarf_query.cppm',
        'source_dwarf_objects.h', 'source_dwarf_objects.cppm', 'source_dwarf_pieces.h', 'source_dwarf_pieces.cppm',
        'source_dwarf_variants.h', 'source_dwarf_variants.cppm', 'source_dwarf_selectors.h', 'source_dwarf_selectors.cppm',
        'source_scope_path.h', 'source_scope_path.cppm')]
    sources += ['test/0017.runtime/' + name for name in (
        'debug_source_dwarf_objects.cc', 'debug_source_dwarf_pieces.cc', 'debug_source_dwarf_selectors.cc', 'debug_source_dwarf_objects_index.cc', 'run_debug_source_dwarf_objects.py')]
    sources += ['test/0017.runtime/fixtures/debug_source_objects_' + name for name in ('c.c', 'cpp.cc', 'rust.rs')]
    sources += ['test/0017.runtime/fixtures/debug_source_pieces_cpp.cc']
    before = {name: sha(root / name) for name in sources}
    (out / 'source-before.json').write_text(json.dumps(before, indent=2) + '\n')
    (out / 'tool-inputs.json').write_text(json.dumps({name: {'path': str(tools[name]), 'sha256': digest}
        for name, digest in tools_before.items()}, indent=2) + '\n')
    rows = []

    def run(name: str, argv: list[str], timeout: int = 120) -> None:
        (out / (name + '.command')).write_text(shlex.join(argv) + '\n')
        with (out / (name + '.log')).open('w') as log:
            completed = subprocess.run(argv, cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
        rows.append({'name': name, 'argv': argv, 'exit_code': completed.returncode})
        (out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
        if completed.returncode:
            raise RuntimeError((out / (name + '.log')).read_text()[-12000:])

    component = out / 'bounded-objects'
    flags = [str(tools['cxx']), '-std=c++23', '-O1', '-g0', '-Wall', '-Wextra', '-Werror',
             '-pthread', '-I' + str(root / 'src'), '-I' + str(root / 'third-parties/fast_io/include'),
             '-I' + str(root / 'third-parties/bizwen/include'), *args.cxxflag]
    run('component-build', [*flags, str(root / 'test/0017.runtime/debug_source_dwarf_objects.cc'), '-o', str(component), *args.ldflag], 300)
    run('component-run', [str(component)])
    pieces = out / 'bounded-pieces'
    run('pieces-build', [*flags, str(root / 'test/0017.runtime/debug_source_dwarf_pieces.cc'), '-o', str(pieces), *args.ldflag], 300)
    run('pieces-run', [str(pieces)])
    selectors = out / 'bounded-selectors'
    run('selectors-build', [*flags, str(root / 'test/0017.runtime/debug_source_dwarf_selectors.cc'), '-o', str(selectors), *args.ldflag], 300)
    run('selectors-run', [str(selectors)])
    fixture_rows = []
    for language in ('c', 'cpp', 'rust'):
        suffix = {'c': 'c', 'cpp': 'cc', 'rust': 'rs'}[language]
        source = root / ('test/0017.runtime/fixtures/debug_source_objects_' + language + '.' + suffix)
        obj = out / (language + '.o')
        wasm = out / (language + '.wasm')
        if language == 'rust':
            command = [str(tools['rustc']), str(source), '--target=wasm32-unknown-unknown', '--edition=2024',
                       '--crate-name=uwvm_debug_source_objects_rust', '--crate-type=cdylib', '--emit=obj',
                       '-Copt-level=0', '-Cdebuginfo=2', '-Cdwarf-version=5', '-Cpanic=abort', '-o', str(obj)]
        else:
            command = [str(tools['clang']), '--target=wasm32-unknown-unknown', '-std=' + ('c11' if language == 'c' else 'c++20'),
                       '-O0', '-g', '-gdwarf-5', '-ffreestanding', '-nostdlib']
            if language == 'cpp':
                command += ['-fno-exceptions', '-fno-rtti']
            command += ['-c', str(source), '-o', str(obj)]
        run(language + '-compile', command, 300)
        run(language + '-link', [str(tools['wasm-ld']), '--no-entry', '--export-all', str(obj), '-o', str(wasm)])
        run(language + '-official-validate', [str(tools['wasm-tools']), 'validate', str(wasm)])
        run(language + '-real-dwarf-layout', [str(tools['metadata-executable']), str(wasm), language])
        fixture_rows.append({'language': language, 'source_sha256': sha(source), 'object_sha256': sha(obj), 'wasm_sha256': sha(wasm)})
    source = root / 'test/0017.runtime/fixtures/debug_source_pieces_cpp.cc'
    obj = out / 'cpp-pieces.o'; wasm = out / 'cpp-pieces.wasm'
    run('cpp-pieces-compile', [str(tools['clang']), '--target=wasm32-unknown-unknown', '-std=c++20', '-O2', '-g', '-gdwarf-5',
        '-ffreestanding', '-nostdlib', '-fno-exceptions', '-fno-rtti', '-c', str(source), '-o', str(obj)], 300)
    run('cpp-pieces-link', [str(tools['wasm-ld']), '--no-entry', '--export-all', str(obj), '-o', str(wasm)])
    run('cpp-pieces-official-validate', [str(tools['wasm-tools']), 'validate', str(wasm)])
    run('cpp-pieces-real-dwarf', [str(tools['metadata-executable']), str(wasm), 'cpp-pieces'])
    fixture_rows.append({'language': 'cpp-pieces', 'source_sha256': sha(source), 'object_sha256': sha(obj), 'wasm_sha256': sha(wasm)})
    after = {name: sha(root / name) for name in sources}
    (out / 'source-after.json').write_text(json.dumps(after, indent=2) + '\n')
    if after != before or any(sha(path) != tools_before[name] for name, path in tools.items()):
        raise RuntimeError('candidate source/tool/executable changed during qualification')
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    (out / 'summary.json').write_text(json.dumps({'passed': True, 'source_before_equals_after': True,
        'tools_before_equals_after': True, 'source': before, 'tools': tools_before, 'commands': rows, 'fixtures': fixture_rows,
        'scope': 'bounded copied object/selectors semantics and actual C/C++/Rust DWARF5 aggregate/array/enum/pointer selector layout',
        'metadata_executable_link_closure_qualified_by_runner': False, 'production_runtime_guest_memory_read_qualified': False,
        'gdb_lldb_parity_qualified': False}, indent=2) + '\n')
    print('PASS bounded copied object components and actual C/C++/Rust DWARF5 layouts')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Keeper-only finite variant/global components and linked real DWARF producers.

All native invocations require the existing 64 GiB/swap0 Linux cgroup. The
keeper must first build and pin the metadata executable with the exact fresh
candidate LLVM-DWARF link closure. No production memory bridge or GDB parity is
qualified by this metadata recipe.
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
    for name in ('cxx', 'clang', 'wasm-ld', 'rustc', 'wasm-tools', 'metadata-executable'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--cxxflag', action='append', default=[])
    parser.add_argument('--ldflag', action='append', default=[])
    parser.add_argument('--metadata-executable-sha256', required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'linux':
        raise RuntimeError('native recipe is remote Linux only')
    root = args.source_root.resolve(strict=True)
    if Path(__file__).resolve(strict=True) != root / 'test/0017.runtime/run_debug_source_dwarf_variants.py':
        raise RuntimeError('runner must belong to exact candidate source root')
    subgroup = root / 'tools/ci/require_wasm3_test_cgroup.sh'
    subprocess.run(['bash', str(subgroup)], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    tools = {name: getattr(args, name.replace('-', '_')).resolve(strict=True)
             for name in ('cxx', 'clang', 'wasm-ld', 'rustc', 'wasm-tools', 'metadata-executable')}
    tool_before = {name: sha(path) for name, path in tools.items()}
    if tool_before['metadata-executable'] != args.metadata_executable_sha256.lower():
        raise RuntimeError('metadata executable bytes differ from keeper pin')
    if any(value.startswith('@') for value in args.cxxflag + args.ldflag):
        raise RuntimeError('unlisted compiler response-file inputs are forbidden')
    sources = ['src/uwvm2/uwvm/debugger/' + name for name in (
        'source_dwarf_types.h', 'source_dwarf_types.cppm', 'source_dwarf_index.h', 'source_dwarf_index.cppm',
        'source_dwarf_query.h', 'source_dwarf_query.cppm', 'source_scope_path.h', 'source_scope_path.cppm',
        'source_dwarf_values.h', 'source_dwarf_values.cppm', 'source_dwarf_pieces.h',
        'source_dwarf_pieces.cppm', 'source_dwarf_objects.h', 'source_dwarf_objects.cppm',
        'source_dwarf_variants.h', 'source_dwarf_variants.cppm', 'source_dwarf_selectors.h', 'source_dwarf_selectors.cppm')]
    sources += ['test/0017.runtime/' + name for name in (
        'debug_source_dwarf_variants.cc', 'debug_source_dwarf_globals.cc', 'debug_source_dwarf_selectors.cc', 'debug_source_dwarf_variants_index.cc',
        'run_debug_source_dwarf_variants.py')]
    sources += ['test/0017.runtime/fixtures/debug_source_' + name for name in (
        'variants_rust.rs', 'globals_c.c', 'globals_cpp.cc', 'globals_rust.rs')]
    before = {name: sha(root / name) for name in sources}
    (out / 'source-before.json').write_text(json.dumps(before, indent=2) + '\n')
    (out / 'tool-inputs.json').write_text(json.dumps({name: {'path': str(tools[name]), 'sha256': digest}
        for name, digest in tool_before.items()}, indent=2) + '\n')
    rows = []

    def run(name: str, argv: list[str], timeout: int = 120) -> None:
        (out / (name + '.command')).write_text(shlex.join(argv) + '\n')
        with (out / (name + '.log')).open('w') as log:
            completed = subprocess.run(argv, cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
        rows.append({'name': name, 'argv': argv, 'exit_code': completed.returncode})
        (out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
        if completed.returncode:
            raise RuntimeError((out / (name + '.log')).read_text()[-12000:])

    flags = [str(tools['cxx']), '-std=c++23', '-O1', '-g0', '-Wall', '-Wextra', '-Werror', '-pthread',
             '-I' + str(root / 'src'), '-I' + str(root / 'third-parties/fast_io/include'),
             '-I' + str(root / 'third-parties/bizwen/include'), *args.cxxflag]
    for name in ('variants', 'globals', 'selectors'):
        exe = out / ('bounded-' + name)
        run(name + '-build', [*flags, str(root / ('test/0017.runtime/debug_source_dwarf_' + name + '.cc')),
                             '-o', str(exe), *args.ldflag], 300)
        run(name + '-run', [str(exe)])
    fixtures = []
    for producer, dwarf_version in [('c', 4), ('c', 5), ('cpp', 4), ('cpp', 5), ('rust', 5), ('variants', 5)]:
        label = 'rust-variants' if producer == 'variants' else producer + '-globals'
        basename = 'variants_rust.rs' if producer == 'variants' else {'c': 'globals_c.c', 'cpp': 'globals_cpp.cc', 'rust': 'globals_rust.rs'}[producer]
        source = root / ('test/0017.runtime/fixtures/debug_source_' + basename)
        name = label + '-dw' + str(dwarf_version)
        obj = out / (name + '.o')
        wasm = out / (name + '.wasm')
        if producer in ('rust', 'variants'):
            command = [str(tools['rustc']), str(source), '--target=wasm32-unknown-unknown', '--edition=2024',
                '--crate-name=uwvm_debug_source_' + producer, '--crate-type=cdylib', '--emit=obj',
                '-Copt-level=0', '-Cdebuginfo=2', '-Cdwarf-version=5', '-Cpanic=abort', '-o', str(obj)]
        else:
            command = [str(tools['clang']), '--target=wasm32-unknown-unknown', '-std=' + ('c11' if producer == 'c' else 'c++20'),
                '-O0', '-g', '-gdwarf-' + str(dwarf_version), '-ffreestanding', '-nostdlib']
            if producer == 'cpp':
                command += ['-fno-exceptions', '-fno-rtti']
            command += ['-c', str(source), '-o', str(obj)]
        run(name + '-compile', command, 300)
        run(name + '-link', [str(tools['wasm-ld']), '--no-entry', '--export-all', str(obj), '-o', str(wasm)])
        run(name + '-official-validate', [str(tools['wasm-tools']), 'validate', str(wasm)])
        run(name + '-actual-linked-dwarf', [str(tools['metadata-executable']), str(wasm), label])
        fixtures.append({'producer': producer, 'dwarf_version': dwarf_version, 'source_sha256': sha(source),
                         'object_sha256': sha(obj), 'wasm_sha256': sha(wasm)})
    after = {name: sha(root / name) for name in sources}
    (out / 'source-after.json').write_text(json.dumps(after, indent=2) + '\n')
    if after != before or any(sha(path) != tool_before[name] for name, path in tools.items()):
        raise RuntimeError('candidate/tool bytes changed during qualification')
    subprocess.run(['bash', str(subgroup)], check=True)
    (out / 'summary.json').write_text(json.dumps({'passed': True, 'source_before_equals_after': True,
        'tools_before_equals_after': True, 'source': before, 'tools': tool_before, 'commands': rows, 'fixtures': fixtures,
        'scope': 'finite variant/copied-known-bits/global metadata and actual relocated DWARF4/5 producer metadata',
        'production_runtime_guest_memory_read_qualified': False, 'continuation_recovery_qualified': False,
        'gdb_lldb_parity_qualified': False}, indent=2) + '\n')
    print('PASS finite variant/global components and actual linked DWARF4/5 producer metadata')


if __name__ == '__main__':
    main()

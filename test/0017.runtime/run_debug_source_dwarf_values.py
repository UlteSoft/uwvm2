#!/usr/bin/env python3
"""Qualify direct numeric copies against a fresh real runtime object.

Run only on Linux within the required existing 64GiB/swap0 test cgroup.
This does not build/qualify the production LLVM-DWARF link closure or execute
DWARF expressions. The real native witness uses the product's actual owning
CLI parse/initializer, fused full validator, engine publisher and stop domain.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shlex
import shutil
import subprocess
import sys


def sha(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--runtime-build', type=Path, required=True,
                        help='Fresh candidate runtime.o and actual test.command in the qualified SDK ABI')
    parser.add_argument('--runtime-object-sha256', required=True,
                        help='Keeper-verified fresh candidate runtime.o SHA; never an old/frozen product object')
    parser.add_argument('--cxx', type=Path, required=True)
    parser.add_argument('--cxxflag', action='append', default=[])
    parser.add_argument('--ldflag', action='append', default=[])
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'linux':
        raise RuntimeError('remote Linux/cgroup runner only')
    root = args.source_root.resolve(strict=True)
    if Path(__file__).resolve(strict=True) != root / 'test/0017.runtime/run_debug_source_dwarf_values.py':
        raise RuntimeError('actual runner must come from precisely --source-root')
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    build = args.runtime_build.resolve(strict=True)
    runtime = build / 'runtime.o'
    expected = args.runtime_object_sha256.lower()
    if len(expected) != 64 or any(c not in '0123456789abcdef' for c in expected) or sha(runtime) != expected:
        raise RuntimeError('runtime object provenance mismatch')
    component_cxx = args.cxx.resolve(strict=True)
    wasm_tool = args.wasm_tools.resolve(strict=True)
    component_cxx_sha = sha(component_cxx)
    wasm_tool_sha = sha(wasm_tool)
    rows = []
    sources = ['src/uwvm2/runtime/lib/uwvm_runtime.default.cpp', 'src/uwvm2/runtime/lib/uwvm_runtime.h',
               'src/uwvm2/runtime/lib/uwvm_runtime_debug_source_api.h',
               'src/uwvm2/runtime/lib/uwvm_runtime_debug_source_binding.h',
               'src/uwvm2/utils/thread/cooperative_pause_domain.h',
               'src/uwvm2/uwvm/debugger/source_dwarf_query.h',
               'test/0017.runtime/debug_source_dwarf_query.cc',
               'test/0017.runtime/debug_source_stop_authority.cc',
               'test/0017.runtime/debug_source_binding_runtime.cc',
               'test/0017.runtime/run_debug_source_dwarf_values.py',
               'src/uwvm2/uwvm/debugger/source_dwarf_types.h',
               'src/uwvm2/uwvm/debugger/source_dwarf_values.h',
               'src/uwvm2/uwvm/debugger/command.h',
               'src/uwvm2/uwvm/debugger/controller.h',
               'src/uwvm2/uwvm/debugger/console.h',
               'test/0017.runtime/debug_source_dwarf_types.cc',
               'test/0017.runtime/debug_source_dwarf_values.cc',
               'test/0017.runtime/debug_source_dwarf_values_console.cc',
               'test/0017.runtime/debug_source_dwarf_values_runtime.cc',
               'test/0017.runtime/fixtures/debug_source_values_nondefaultable.wat']
    sources += ['src/uwvm2/uwvm/run/owned_source.h', 'src/uwvm2/uwvm/runtime/storage/full.h']
    for stem in ('source_dwarf_types', 'source_scope_path', 'source_dwarf_query', 'source_dwarf_variants',
                 'source_dwarf_pieces', 'source_dwarf_values', 'source_dwarf_objects',
                 'source_dwarf_selectors', 'source_dwarf_index'):
        for suffix in ('.h', '.cppm'):
            dependency = 'src/uwvm2/uwvm/debugger/' + stem + suffix
            if dependency not in sources:
                sources.append(dependency)
    source_before = {p: sha(root / p) for p in sources}
    (out / 'source-inputs.json').write_text(json.dumps(source_before, indent=2) + '\n')

    def run(name: str, command: list[str], timeout: int = 90) -> None:
        (out / (name + '.command')).write_text(shlex.join(command) + '\n')
        with (out / (name + '.log')).open('w') as log:
            result = subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
        rows.append({'name': name, 'command': command, 'exit_code': result.returncode})
        (out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
        if result.returncode:
            raise RuntimeError((out / (name + '.log')).read_text()[-10000:])

    flags = [str(component_cxx), '-std=c++23', '-O1', '-g0', '-Wall', '-Wextra', '-Werror', '-DUWVM=2',
             '-pthread', '-I' + str(root / 'src'), '-I' + str(root / 'third-parties/fast_io/include'),
             '-I' + str(root / 'third-parties/bizwen/include'), *args.cxxflag]
    for name in ('debug_source_dwarf_types', 'debug_source_dwarf_values', 'debug_source_dwarf_values_console', 'debug_source_dwarf_query', 'debug_source_stop_authority'):
        run(name + '-build', [*flags, str(root / ('test/0017.runtime/' + name + '.cc')),
                             '-o', str(out / name), *args.ldflag], 180)
        run(name + '-run', [str(out / name)])
    # Reuse the actual runtime object's qualified full/native ABI and component
    # libraries. This changes only the native witness source/output, no frozen
    # object/archive, production source or runtime linker flags.
    command_file = build / 'test.command'
    command_text = command_file.read_text()
    command = shlex.split(command_text)
    if not command or command.count('-o') != 1 or any(value.startswith('@') for value in command):
        raise RuntimeError('one direct compiler invocation/output required; hidden response-file arguments refused')
    side_outputs = ('-MF', '-MJ', '-MD', '-MMD', '-save-temps', '--serialize-diagnostics',
                    '-fmodule-output', '-fmodules-cache-path', '-Wl,-Map', '-Wl,--Map', '-Wl,--output')
    if any(value.startswith(side_outputs) or value == '-dependency-file' for value in command):
        raise RuntimeError('old compile command has side outputs; keeper must provide a fresh direct witness link command')
    object_inputs = [Path(value) if Path(value).is_absolute() else root / value
                     for value in command if not value.startswith('-') and Path(value).name == runtime.name]
    if len(object_inputs) != 1 or object_inputs[0].resolve(strict=True) != runtime.resolve(strict=True):
        raise RuntimeError('actual compiler argv must link precisely the qualified runtime.o path')
    compiler = Path(command[0]) if Path(command[0]).is_absolute() else Path(shutil.which(command[0]) or '')
    compiler = compiler.resolve(strict=True)
    command[0] = str(compiler)  # execute the exact resolved compiler whose bytes are recorded below.
    (out / 'actual-original-test.command').write_text(command_text)
    build_receipts = {name: sha(build / name) for name in ('source-before.json', 'source-after.json',
                      'runtime.command', 'build.command', 'compile.command') if (build / name).is_file()}
    tool_inputs = {'component_cxx': component_cxx_sha, 'actual_witness_cxx': sha(compiler),
                   'wasm_tools': wasm_tool_sha, 'actual_test_command': sha(command_file)}
    (out / 'build-inputs.json').write_text(json.dumps({'tools': tool_inputs, 'actual_linked_runtime_object': str(runtime.resolve()),
        'verified_object_sha256': expected, 'keeper_build_receipt_sha256': build_receipts,
        'source_build_closure_qualified_by_runner': False}, indent=2) + '\n')
    candidates = [i for i, value in enumerate(command)
                  if value.endswith('.cc') and ('test/' in value or Path(value).name.startswith('fixture'))]
    if len(candidates) != 1 or '-o' not in command:
        raise RuntimeError('need one unambiguous actual test.command witness source/output')
    command[candidates[0]] = str(root / 'test/0017.runtime/debug_source_dwarf_values_runtime.cc')
    new_output = out / 'runtime-witness'
    if new_output.exists() or new_output.resolve() == runtime.resolve() or any(
            Path(value).exists() and Path(value).resolve() == new_output.resolve() for value in command if not value.startswith('-')):
        raise RuntimeError('witness output would overwrite an old input')
    command[command.index('-o') + 1] = str(new_output)
    run('runtime-witness-build', command, 300)
    wat = out / 'fixture.wat'
    wat.write_text('(module (func $helper (param i32) (result i32) (local i32) '
                   'local.get 0 i32.const 2 i32.add local.tee 1) '
                   '(func (export "_start") (result i32) i32.const 5 call $helper))\n')
    run('fixture-parse', [str(wasm_tool), 'parse', str(wat), '-o', str(out / 'fixture.wasm')])
    run('fixture-validate', [str(wasm_tool), 'validate', str(out / 'fixture.wasm')])
    for policy in ('instruction', 'unwind'):
        run('runtime-' + policy, [str(out / 'runtime-witness'), str(out / 'fixture.wasm'), policy], 90)
    nondefaultable = out / 'nondefaultable.wasm'
    run('nondefaultable-parse', [str(wasm_tool), 'parse',
        str(root / 'test/0017.runtime/fixtures/debug_source_values_nondefaultable.wat'), '-o', str(nondefaultable)])
    run('nondefaultable-validate', [str(wasm_tool), 'validate', str(nondefaultable)])
    for policy in ('instruction', 'unwind'):
        run('runtime-availability-' + policy, [str(out / 'runtime-witness'), str(nondefaultable), policy, 'availability'], 90)
    if sha(runtime) != expected or sha(command_file) != tool_inputs['actual_test_command'] or \
       sha(component_cxx) != component_cxx_sha or sha(wasm_tool) != wasm_tool_sha or \
       sha(compiler) != tool_inputs['actual_witness_cxx'] or \
       any(sha(build / name) != digest for name, digest in build_receipts.items()):
        raise RuntimeError('runtime object/original command/compiler/tool/build receipt changed during qualification')
    source_after = {p: sha(root / p) for p in sources}
    (out / 'source-after.json').write_text(json.dumps(source_after, indent=2) + '\n')
    if source_after != source_before:
        raise RuntimeError('source candidate changed during qualification')
    subprocess.run(['bash', str(root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    (out / 'summary.json').write_text(json.dumps({'passed': True, 'runtime_object_sha256': expected,
        'sources': source_before, 'source_before_equals_after': True, 'tools': tool_inputs, 'commands': rows,
        'keeper_build_receipt_sha256': build_receipts, 'source_build_closure_qualified_by_runner': False,
        'scope': 'Real native numeric parameter/local copies, Core3 nondefaultable readability flags/original indices, and full-entry generations with actual source/stop/alias/replacement/reset authority; finite synthetic query/formatter components.',
        'official_dwarf_locations_qualified': False, 'kernel_native_step_qualified': False,
        'production_dwarf_link_closure_qualified': False}, indent=2) + '\n')
    print('PASS finite direct numeric copies: actual bridge generations, real source/stop/replacement/reset; synthetic query and formatter')


if __name__ == '__main__':
    main()

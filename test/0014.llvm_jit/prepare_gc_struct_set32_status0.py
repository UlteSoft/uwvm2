#!/usr/bin/env python3
"""Source-only, fail-closed preparation; never run a compiler, WAT tool or VM."""
import argparse
import ast
import hashlib
import json
from pathlib import Path

BASE = 'build/wasm3-evidence/source-only-set32-status0-before-20261002-v1'
DIRECTORY = 'src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/'
EMITTER = DIRECTORY + 'single_func_gc_struct_set32.h'
CACHE = 'src/uwvm2/runtime/llvm_jit_cache/environment.h'
GATE = '#if defined(UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32) && UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32 == 1'
OLD_TESTS = ['test/0014.llvm_jit/llvm_jit_gc_struct_set32_ir.cc',
             'test/0014.llvm_jit/llvm_jit_gc_struct_set32_cache_key.cc']
TESTS = ['test/0014.llvm_jit/llvm_jit_gc_struct_set32_status0_ir.cc',
         'test/0014.llvm_jit/llvm_jit_gc_struct_set32_status0_cache_key.cc',
         'test/0014.llvm_jit/GC_STRUCT_SET32_STATUS0_20261002.md',
         'test/0014.llvm_jit/prepare_gc_struct_set32_status0.py']
WAT = ['test/0014.llvm_jit/fixtures/gc_struct_set32_' + name + '.wat'
       for name in ('scalar', 'null', 'immutable_invalid', 'field_invalid', 'fallback')]
RETAINED = [DIRECTORY + 'single_func_gc_emit.h',
            DIRECTORY + 'single_func_gc_struct_set32_bridge.h',
            'src/uwvm2/uwvm/runtime/storage/gc_object.h',
            'src/uwvm2/uwvm/runtime/storage/compact_numeric/descriptor.h',
            'src/uwvm2/runtime/compiler/shared/wasm_exception_private_leaf_effect.h',
            'test/0017.runtime/wasm3_gc_struct_set32_bridge.cc']


def sha(data):
    return hashlib.sha256(data).hexdigest()


def off_projection(data):
    result, depth, count = [], 0, 0
    for line in data.decode().splitlines(keepends=True):
        stripped = line.strip()
        if depth:
            if stripped.startswith(('#if ', '#ifdef ', '#ifndef ')):
                depth += 1
            elif stripped.startswith('#endif'):
                depth -= 1
            elif depth == 1 and stripped.startswith(('#else', '#elif')):
                raise ValueError('candidate gate has an unexpected alternative')
        elif stripped == GATE:
            depth = 1
            count += 1
        else:
            result.append(line)
    if depth:
        raise ValueError('unterminated exact-1 gate')
    return ''.join(result).encode(), count


def prepare(root, out):
    before = root / BASE
    manifest = json.loads((before / 'manifest.json').read_text())
    if manifest['product'] != root.name or manifest['source_only'] is not True:
        raise ValueError('wrong before-image owner')
    for relative, pin in manifest['files'].items():
        data = (before / 'snapshot' / relative).read_bytes()
        if len(data) != pin['bytes'] or sha(data) != pin['sha256']:
            raise ValueError('before-image bytes do not match')
    cache_before = (before / 'snapshot' / CACHE).read_bytes()
    cache_after = (root / CACHE).read_bytes()
    old = b'u8"llvm-gc-struct-set32-abi", u8"registerwide-v1"'
    new = b'u8"llvm-gc-struct-set32-abi", u8"registerwide-status0-cold-v2"'
    if cache_before.count(old) != 1 or cache_before.replace(old, new, 1) != cache_after:
        raise ValueError('cache mutation exceeds the approved single version value')
    projected_before, blocks = off_projection(cache_before)
    projected_after, after_blocks = off_projection(cache_after)
    if projected_before != projected_after or blocks != after_blocks or blocks != 1:
        raise ValueError('cache default-off text differs')
    aggregate = (root / RETAINED[0]).read_bytes()
    aggregate_off, aggregate_blocks = off_projection(aggregate)
    if aggregate_blocks != 2 or b'single_func_gc_struct_set32.h' in aggregate_off or \
            b'try_emit_runtime_local_func_llvm_jit_gc_struct_set32(' in aggregate_off:
        raise ValueError('setter fragment is reachable outside its exact-1 gates')
    for relative in OLD_TESTS:
        if (root / relative).read_bytes() != (before / 'snapshot' / relative).read_bytes():
            raise ValueError('historical nine-case/cache source changed')
    inputs = [EMITTER, CACHE] + TESTS + OLD_TESTS + WAT + RETAINED
    pins, data_by_file = {}, {}
    for relative in inputs:
        data = (root / relative).read_bytes()
        data_by_file[relative] = data
        pins[relative] = {'sha256': sha(data), 'bytes': len(data)}
    ast.parse(data_by_file[TESTS[-1]], filename=TESTS[-1])
    plan = {'schema': 'uwvm-set32-status0-cold-source-v1', 'product': root.name,
            'source_only': True, 'execute_ready': False, 'formal_acceptance': False,
            'compiler_passed': False, 'native_passed': False, 'whole_vm_passed': False,
            'assembly_passed': False, 'performance_passed': False,
            'runtime_abi_unchanged_by_candidate': True,
            'exact_macro': 'UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32=1',
            'cache_revision': 'registerwide-status0-cold-v2',
            'compiler_unit': TESTS[0], 'cache_unit': TESTS[1],
            'cache_macro_values': ['undefined', 0, 2, 1],
            'compiler_cases': 9, 'scalar_cases': 4, 'fallback_cases': 5,
            'actual_required': ['aligned transitive header/runtime/SDK source closure',
                                'all nine input/O3 IR cases and native object disassembly',
                                'success has one machine status guard before original return',
                                'error calls original trap ABI/NoTail/Win64 context',
                                'unchanged real setter native equivalence, stale/foreign/concurrent controls',
                                'five official Core 3 WAT fixtures and identical Wasmtime bytes',
                                'complete CLI/mmap safety before mutation-family performance'],
            'wats': WAT, 'semantic_native_unit': RETAINED[-1],
            'reuse_runtime_requires_actual_transitive_closure': True,
            'build_source_identity': {'compiler': None, 'runtime': None, 'SDK': None, 'ELF': None,
                                      'external_bound_ID_allowed': True, 'single_TU_ID_injection_allowed': False},
            'resource': {'sole_keeper': '/root/linux_fused_resume', 'memory_bytes': 64 * 1024**3,
                         'swap_bytes': 0, 'compile_cpu': 'allowed E cores',
                         'performance_cpu': 'P0', 'temperature': 'observation only',
                         'frequency_and_host_sibling_evidence': 'required',
                         'compile_VM_profiler_overlap_allowed': False},
            'files': pins}
    receipt = {'scope': 'source-only text/AST/hash; no preprocessing/native/VM/assembly pass',
               'before_manifest_sha256': sha((before / 'manifest.json').read_bytes()),
               'cache_default_off_byte_equal': True, 'cache_default_off_sha256': sha(projected_after),
               'cache_changed_only_single_key_value': True,
               'setter_exact1_selection_blocks': aggregate_blocks,
               'setter_absent_from_macro_off_text': True,
               'historical_test_source_byte_equal': True, 'source_before_after_equal': False,
               'retained_files_are_pins_not_an_independent_full_source_recipe': True,
               'files': pins}
    out.mkdir(parents=True, exist_ok=False)
    # Small actual source snapshot only: retained runtime/GC is pinned, not copied.
    for relative in [EMITTER, CACHE] + TESTS:
        path = out / 'snapshot' / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data_by_file[relative])
    for relative, data in data_by_file.items():
        if (root / relative).read_bytes() != data:
            raise ValueError('live source changed during preparation')
    receipt['source_before_after_equal'] = True
    (out / 'cold-plan.json').write_text(json.dumps(plan, indent=2) + '\n')
    (out / 'source-only-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return {'plan_sha256': sha((out / 'cold-plan.json').read_bytes()),
            'receipt_sha256': sha((out / 'source-only-receipt.json').read_bytes())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    out = args.out.resolve()
    if any(out.is_relative_to(root / directory) for directory in ('src', 'third-parties', 'test')):
        raise ValueError('evidence output cannot modify source directories')
    print(json.dumps(prepare(root, out), indent=2))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Source-only preparation. Does not execute WAT tools, compilers or VMs.

The native baseline is a verbatim original production prefix. Exact-macro-off
projection is textual equivalence only, never a preprocessing/assembly result.
The keeper must bind actual source/compiler/runtime/ELF and execute all gates.
"""
import argparse
import hashlib
import json
from pathlib import Path

GATE = '#if defined(UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32) && UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32 == 1'
DIR = 'src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/'
PRODUCTION = [DIR + 'single_func_gc_emit.h', DIR + 'single_func_gc_struct_set32.h',
              DIR + 'single_func_gc_struct_set32_bridge.h',
              'src/uwvm2/runtime/llvm_jit_cache/environment.h']
TESTS = ['test/0017.runtime/wasm3_gc_struct_set32_bridge.cc',
         'test/0014.llvm_jit/llvm_jit_gc_struct_set32_ir.cc',
         'test/0014.llvm_jit/llvm_jit_gc_struct_set32_cache_key.cc',
         'test/0014.llvm_jit/prepare_gc_struct_set32.py',
         'test/0014.llvm_jit/GC_STRUCT_SET32_CANDIDATE_20261002.md'] + [
    'test/0014.llvm_jit/fixtures/gc_struct_set32_' + name + '.wat' for name in
    ('scalar', 'null', 'immutable_invalid', 'field_invalid', 'fallback')]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def off_projection(data):
    """Remove only whole new exact-1 blocks, preserving every other byte."""
    lines = data.decode().splitlines(keepends=True)
    result, count, depth = [], 0, 0
    for line in lines:
        stripped = line.strip()
        if depth:
            if stripped.startswith(('#if ', '#ifdef ', '#ifndef ')):
                depth += 1
            elif stripped.startswith('#endif'):
                depth -= 1
            elif depth == 1 and stripped.startswith(('#else', '#elif')):
                raise ValueError('new gate unexpectedly has an alternative branch')
        elif stripped == GATE:
            depth = 1
            count += 1
        else:
            result.append(line)
    if depth:
        raise ValueError('unterminated candidate gate')
    return ''.join(result).encode(), count


def prepare(root, out):
    before_dir = root / 'build/wasm3-evidence/source-only-struct-set32-before-20261002'
    before = json.loads((before_dir / 'manifest.json').read_text())
    receipt = {'scope': 'source-only; textual projection, no native/compiler/VM/performance pass',
               'product': root.name, 'formal_acceptance': False, 'native_passed': False,
               'compiler_passed': False, 'whole_vm_passed': False, 'performance_passed': False,
               'macro': 'UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32', 'enabled_value': 1,
               'six_switch_profile_does_not_enable_candidate': True,
               'projection': {}, 'pins_before': {}}
    for relative in PRODUCTION + TESTS:
        data = (root / relative).read_bytes()
        receipt['pins_before'][relative] = {'bytes': len(data), 'sha256': sha(data)}
    for relative in (PRODUCTION[0], PRODUCTION[3]):
        data = (root / relative).read_bytes()
        expected = (before_dir / Path(relative).name).read_bytes()
        projected, count = off_projection(data)
        # The one blank separating the new include block belongs to that block.
        if relative == PRODUCTION[0]:
            projected = projected.replace(b'#include "single_func_gc_array_set32.h"\n#endif\n\n\n',
                b'#include "single_func_gc_array_set32.h"\n#endif\n\n', 1)
        if projected != expected or sha(expected) != before['files'][relative]['sha256']:
            raise ValueError('default-off source projection differs from the pinned before-image')
        receipt['projection'][relative] = {'candidate_blocks': count, 'sha256': sha(projected),
                                           'exact_before_image_bytes': True, 'text_only': True}
    for relative, pin in before['files'].items():
        if pin.get('immutable_freeze_only'):
            data = (root / relative).read_bytes()
            if sha(data) != pin['sha256']:
                raise ValueError('unrelated frozen GC production bytes drifted')
            receipt['pins_before'][relative] = {'bytes': len(data), 'sha256': sha(data), 'unchanged': True}
    source = (root / PRODUCTION[0]).read_text()
    start = "template<::std::uint_least32_t FixedOpcode = 0xffff'ffffu, ::std::size_t FixedInputs = SIZE_MAX>"
    stop = '\n#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)\ntemplate<::std::uint_least32_t Opcode, ::std::size_t Inputs>'
    if source.count(start) != 1 or source.count(stop) != 1:
        raise ValueError('original native helper markers changed')
    prefix = source[source.index(start):source.index(stop)]
    if '::llvm::' in prefix or prefix.count('llvm_jit_gc_aggregate_fixed_bridge(') != 1:
        raise ValueError('native prefix is not exactly the original helper')
    out.mkdir(parents=True, exist_ok=False)
    header = out / 'native_gc_struct_set32_baseline.h'
    header.write_text(prefix)
    receipt['native_baseline_extraction'] = {'file': header.name, 'bytes': header.stat().st_size,
        'sha256': sha(header.read_bytes()), 'verbatim_original_helper': True}
    original = (root / 'test/0014.llvm_jit/fixtures/gc_struct_set32_scalar.wat').read_text()
    anchor = '(struct.get $base 0 (local.get $r)) (i32.const 0xabcdef12)'
    if original.count(anchor) != 1:
        raise ValueError('independent readback-control anchor is not unique')
    bad = original.replace(anchor, '(struct.get $base 0 (local.get $r)) (i32.const 0xabcdef13)')
    bad_path = out / 'gc_struct_set32_scalar_bad_expected.wat'
    bad_path.write_text(bad)
    receipt['bad_expected_readback_control'] = {'file': bad_path.name,
        'sha256': sha(bad_path.read_bytes()), 'expected': 'valid Core 3 module; runtime unreachable',
        'only_observer_changed': True, 'no_oracle_run': True}
    for relative, pin in receipt['pins_before'].items():
        if sha((root / relative).read_bytes()) != pin['sha256']:
            raise ValueError('source changed during preparation')
    receipt['source_before_after_equal'] = True
    (out / 'source-only-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    out = args.out.resolve()
    if any(out.is_relative_to(root / name) for name in ('src', 'third-parties')):
        raise ValueError('preparation may not write production input directories')
    print(json.dumps(prepare(root, out), indent=2))


if __name__ == '__main__':
    main()

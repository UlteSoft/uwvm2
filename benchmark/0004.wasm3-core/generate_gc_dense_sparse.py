#!/usr/bin/env python3
"""Pure Core3 dense/sparse GC graph producer; never parses or runs a Wasm VM.

Its all-reference/i31 dense graph matches the native dense scanner fixture's
canonical layout, LCG payloads, edge counts, allocation count and root checksum.
Native explicit collections and VM automatic collections are distinct schedules.
"""
import argparse
import hashlib
import json
from pathlib import Path

MASK = (1 << 32) - 1
ROOTS = 1024
BATCH = 4096
SEED = 123456789


def oracle(density, iterations):
    if density not in ('sparse', 'dense') or type(iterations) is not int:
        raise ValueError('Unknown density or non-integral iterations')
    if iterations % BATCH or not BATCH <= iterations <= 256 * BATCH:
        raise ValueError('Iterations must be a whole 4096 batch in [4096,1048576]')
    state = SEED
    ring = [0] * ROOTS
    total = 0
    for index in range(iterations):
        state = (state * 1664525 + 1013904223) & MASK
        payload = state & 0x7FFFFFFF if density == 'dense' else state
        total = (total + payload) & MASK
        ring[index & (ROOTS - 1)] = payload
    root_sum = sum(ring) & MASK
    return dict(step_checksum_u32=total, root_checksum_u32=root_sum,
                return_checksum_u32=total ^ root_sum, last_lcg_u32=state,
                guest_planned_allocations=iterations, final_reachable_objects=ROOTS,
                actual_collection_count=None, actual_reclaimed_count=None)


def signed32(value):
    return value if value < 1 << 31 else value - (1 << 32)


def build(density, width, iterations):
    if width not in (64, 65):
        raise ValueError('Width must be exactly 64 or 65')
    expected = oracle(density, iterations)
    dense = density == 'dense'
    fields = ['(field (mut (ref null eq)))' if dense or field == width - 1
              else '(field (mut i32))' for field in range(width)]
    first_edge = 1 if dense else width - 1
    store_edges = ''.join('      local.get $current\n      local.get $current\n'
                         f'      struct.set $node {field}\n' for field in range(first_edge, width))
    check_edges = ''.join('      local.get $current\n'
                         f'      struct.get $node {field}\n      local.get $current\n'
                         '      ref.eq\n      i32.eqz\n      if unreachable end\n'
                         for field in range(first_edge, width))
    box = '      ref.i31\n' if dense else ''
    unbox = '      ref.cast (ref i31)\n      i31.get_u\n' if dense else ''
    wat = f'''(module
  (rec (type $node (struct
    {' '.join(fields)})))
  (table $roots {ROOTS} (ref null $node))
  (func $run (export "run") (result i32)
    (local $i i32) (local $state i32) (local $total i32)
    (local $root_sum i32) (local $current (ref null $node))
    i32.const {SEED}
    local.set $state
    loop $allocate
      local.get $state
      i32.const 1664525
      i32.mul
      i32.const 1013904223
      i32.add
      local.set $state
      struct.new_default $node
      local.set $current
      local.get $current
      local.get $state
{box}      struct.set $node 0
{store_edges}      local.get $i
      i32.const {ROOTS - 1}
      i32.and
      local.get $current
      table.set $roots
      local.get $total
      local.get $current
      struct.get $node 0
{unbox}      i32.add
      local.set $total
      local.get $i
      i32.const 1
      i32.add
      local.tee $i
      i32.const {iterations}
      i32.lt_u
      br_if $allocate
    end
    i32.const 0
    local.set $i
    loop $check_roots
      local.get $i
      table.get $roots
      local.set $current
{check_edges}      local.get $root_sum
      local.get $current
      struct.get $node 0
{unbox}      i32.add
      local.set $root_sum
      local.get $i
      i32.const 1
      i32.add
      local.tee $i
      i32.const {ROOTS}
      i32.lt_u
      br_if $check_roots
    end
    local.get $total
    i32.const {signed32(expected['step_checksum_u32'])}
    i32.ne
    if unreachable end
    local.get $root_sum
    i32.const {signed32(expected['root_checksum_u32'])}
    i32.ne
    if unreachable end
    local.get $total
    local.get $root_sum
    i32.xor)
  (func (export "_start")
    call $run
    i32.const {signed32(expected['return_checksum_u32'])}
    i32.ne
    if unreachable end))
'''
    data = wat.encode()
    return data, dict(schema='uwvm-gc-dense-sparse-wat-v2', density=density,
        field_count=width, reference_field_count=width if dense else 1,
        heap_self_edges_per_object=width - 1 if dense else 1,
        nonheap_i31_fields=1 if dense else 0, scalar_kind='i31' if dense else 'i32',
        explicit_recursive_group=True, authentic_self_cycles=True, root_ring=ROOTS,
        iterations=iterations, native_equivalent_rounds=iterations // BATCH,
        graph_layout_and_scalar_oracle_match=True, native_and_vm_timing_schedule_match=False,
        expected=expected, wat_sha256=hashlib.sha256(data).hexdigest(),
        actual_wasm_sha256=None, official_parser_executed=False,
        native_or_vm_executed=False, performance_accepted=False,
        core3_primary_spec='https://webassembly.github.io/spec/core/exec/instructions.html',
        actual_managed_roots_and_reclamation_qualification=None,
        compact_numeric_eligible=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--density', choices=('sparse', 'dense'), required=True)
    parser.add_argument('--width', type=int, choices=(64, 65), required=True)
    parser.add_argument('--iterations', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or args.output.with_suffix('.json').exists():
        raise RuntimeError('Refusing to replace a frozen fixture or metadata')
    data, metadata = build(args.density, args.width, args.iterations)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    args.output.with_suffix('.json').write_text(json.dumps(metadata, indent=2, allow_nan=False) + '\n')


if __name__ == '__main__':
    main()

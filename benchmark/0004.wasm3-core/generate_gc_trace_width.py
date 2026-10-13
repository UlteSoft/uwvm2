#!/usr/bin/env python3
"""Emit self-checking Core 3 wide cyclic structs; no compiler or VM runs here.

The 64/65 boundary isolates inline trace metadata from its owned-array fallback.
Official wasm-tools parsing and execution remain mandatory in the keeper lane.
"""
import argparse
import hashlib
import json
from pathlib import Path

MASK = 0xFFFFFFFF
SEED = 123456789
ROOTS = 1024


def oracle(iterations):
    if type(iterations) is not int or not ROOTS <= iterations <= 2_000_000:
        raise ValueError("iterations must be in [1024, 2000000]")
    ring = [0] * ROOTS
    state = SEED
    total = 0
    for index in range(iterations):
        state = (1664525 * state + 1013904223) & MASK
        slot = index & (ROOTS - 1)
        if index >= ROOTS:
            total = (total + ring[slot]) & MASK
        ring[slot] = state
        total = (total + state) & MASK
    rootsum = sum(ring) & MASK
    return dict(step_checksum_u32=total, root_checksum_u32=rootsum,
                return_checksum_u32=total ^ rootsum, last_lcg_u32=state,
                guest_planned_allocations=iterations, final_reachable_objects=ROOTS,
                collection_count=None, reclaimed_count=None)


def signed(value):
    return value if value < 1 << 31 else value - (1 << 32)


def build(width, iterations):
    if width not in (64, 65):
        raise ValueError("width must be 64 or 65")
    expected = oracle(iterations)
    fields = ['(field (mut i32))']
    # Numeric SIMD/packed fields deliberately coexist with a defined reference;
    # neither padding nor packed data may be treated as a GC reference edge.
    fields += ['(field (mut ' + ('v128' if i % 3 == 0 else 'i16' if i % 3 == 1 else 'i64') + '))'
               for i in range(1, width - 1)]
    fields += ['(field (mut (ref null $node)))']
    wat = f'''(module
  (rec (type $node (struct
    {' '.join(fields)})))
  (table $roots {ROOTS} (ref null $node))
  (func $run (export "run") (result i32)
    (local $i i32) (local $state i32) (local $total i32)
    (local $rootsum i32) (local $current (ref null $node))
    i32.const {SEED}
    local.set $state
    loop $work
      local.get $state
      i32.const 1664525
      i32.mul
      i32.const 1013904223
      i32.add
      local.set $state
      local.get $i
      i32.const {ROOTS}
      i32.ge_u
      if
        local.get $total
        local.get $i
        i32.const {ROOTS - 1}
        i32.and
        table.get $roots
        struct.get $node 0
        i32.add
        local.set $total
      end
      struct.new_default $node
      local.set $current
      local.get $current
      local.get $state
      struct.set $node 0
      local.get $current
      local.get $current
      struct.set $node {width - 1}
      local.get $current
      struct.get $node {width - 1}
      local.get $current
      ref.eq
      i32.eqz
      if unreachable end
      local.get $i
      i32.const {ROOTS - 1}
      i32.and
      local.get $current
      table.set $roots
      local.get $total
      local.get $current
      struct.get $node 0
      i32.add
      local.set $total
      local.get $i
      i32.const 1
      i32.add
      local.tee $i
      i32.const {iterations}
      i32.lt_u
      br_if $work
    end
    i32.const 0
    local.set $i
    loop $final_roots
      local.get $rootsum
      local.get $i
      table.get $roots
      struct.get $node 0
      i32.add
      local.set $rootsum
      local.get $i
      i32.const 1
      i32.add
      local.tee $i
      i32.const {ROOTS}
      i32.lt_u
      br_if $final_roots
    end
    local.get $total
    i32.const {signed(expected['step_checksum_u32'])}
    i32.ne
    if unreachable end
    local.get $rootsum
    i32.const {signed(expected['root_checksum_u32'])}
    i32.ne
    if unreachable end
    local.get $total
    local.get $rootsum
    i32.xor)
  (func (export "_start")
    call $run
    i32.const {signed(expected['return_checksum_u32'])}
    i32.ne
    if unreachable end))
'''
    data = wat.encode()
    return data, dict(schema='uwvm-gc-trace-width-fixture-v1', field_count=width,
                      reference_field_index=width - 1, packed_and_simd_numeric_fields=True,
                      explicit_recursive_group=True, self_cycle=True, root_ring=ROOTS,
                      iterations=iterations, expected=expected,
                      wat_sha256=hashlib.sha256(data).hexdigest(),
                      actual_wasm_sha256=None, native_or_vm_executed=False,
                      compact_numeric_eligible=False,
                      qualification='actual managed roots/collections/reclamation and independent scalar checks required')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--width', type=int, choices=(64, 65), required=True)
    parser.add_argument('--iterations', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or args.output.with_suffix('.json').exists():
        raise RuntimeError("refusing to replace an existing fixture or metadata")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    data, metadata = build(args.width, args.iterations)
    args.output.write_bytes(data)
    args.output.with_suffix('.json').write_text(json.dumps(metadata, indent=2) + '\n')


if __name__ == '__main__':
    main()

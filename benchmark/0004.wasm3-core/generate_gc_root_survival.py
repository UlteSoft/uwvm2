#!/usr/bin/env python3
"""Generate a self-checking GC-root survival fixture for a future collector.

The fixture deliberately overwrites exported garbage-table roots while one
sentinel survives only in the selected location. Passing with today's
monotonic object store is a semantic preflight, not proof of collection.
"""

import argparse
import hashlib
import json
from pathlib import Path


ROOTS = ("local", "operand", "table", "global", "exception", "spill")


def module(allocations, root, warmup_calls=0):
    if not 0 < allocations <= 0x7FFFFFFF:
        raise ValueError("allocations must fit a positive signed i32")
    if not 0 <= warmup_calls <= 0x3FFFFFFF:
        raise ValueError("warmup calls must fit a safe signed i32 sum")
    if root not in (*ROOTS, "all"):
        raise ValueError(root)
    spill_count = 16
    spill_locals = " ".join(f"(local $r{i} (ref null $node))" for i in range(spill_count))
    spill_initializers = "\n".join(
        f"    i32.const {601 + i} call $make_selected local.set $r{i}"
        for i in range(spill_count))
    spill_reads = "\n".join(
        f"    local.get $r{i} ref.cast (ref $node) struct.get $node 0 i32.add"
        for i in range(spill_count))
    spill_expected = sum(seed + (seed & 1) for seed in range(601, 601 + spill_count))
    calls = ROOTS if root == "all" else (root,)
    invocation = (("    call $warm_tiered\n" if warmup_calls else "") +
                  "\n".join(f"    call ${name}_root" for name in calls))
    return f"""(module
  (type $node (struct (field i32)))
  (type $tick_signature (func (param i32) (result i32)))
  (func $tick0 (type $tick_signature) (param $x i32) (result i32)
    local.get $x i32.const 1 i32.add)
  (func $tick1 (type $tick_signature) (param $x i32) (result i32)
    local.get $x i32.const 2 i32.add)
  (table $ticks (export "ticks") 2 funcref)
  (elem (i32.const 0) func $tick0 $tick1)
  (table $candidates (export "candidates") 2 anyref)
  (table $holder (export "holder") 1 anyref)
  (table $garbage (export "garbage") 1024 anyref)
  (global $root (export "global_root") (mut (ref null $node)) (ref.null $node))
  (tag $payload (param (ref $node)))

  (func $warm_tiered (local $index i32) (local $value i32)
    i32.const 0 local.set $index
    i32.const 0 local.set $value
    loop $again
      local.get $value
      local.get $index i32.const 1 i32.and
      call_indirect (type $tick_signature) local.set $value
      local.get $index i32.const 1 i32.add local.tee $index
      i32.const {warmup_calls} i32.lt_u br_if $again
    end
    local.get $value i32.const {warmup_calls + warmup_calls // 2}
    i32.ne if unreachable end)

  (func $make_selected (param $seed i32) (result (ref $node))
    (local $selected (ref null $node))
    i32.const 0 local.get $seed struct.new $node table.set $candidates
    i32.const 1 local.get $seed i32.const 1 i32.add struct.new $node table.set $candidates
    local.get $seed i32.const 1 i32.and table.get $candidates
    ref.cast (ref $node) local.set $selected
    i32.const 0 ref.null any table.set $candidates
    i32.const 1 ref.null any table.set $candidates
    local.get $selected ref.cast (ref $node))

  (func $stress (local $i i32)
    i32.const 0 local.set $i
    loop $again
      local.get $i i32.const 1023 i32.and
      local.get $i struct.new $node table.set $garbage
      local.get $i i32.const 1 i32.add local.tee $i
      i32.const {allocations} i32.lt_u br_if $again
    end)

  (func $local_root (local $held (ref null $node))
    i32.const 101 call $make_selected local.set $held
    call $stress
    local.get $held ref.cast (ref $node) struct.get $node 0
    i32.const 102 i32.ne if unreachable end)

  (func $operand_root
    i32.const 201 call $make_selected
    call $stress
    struct.get $node 0
    i32.const 202 i32.ne if unreachable end)

  (func $table_root
    i32.const 0 i32.const 302 call $make_selected table.set $holder
    call $stress
    i32.const 0 table.get $holder ref.cast (ref $node) struct.get $node 0
    i32.const 302 i32.ne if unreachable end
    i32.const 0 ref.null any table.set $holder)

  (func $global_root
    i32.const 403 call $make_selected global.set $root
    call $stress
    global.get $root ref.cast (ref $node) struct.get $node 0
    i32.const 404 i32.ne if unreachable end
    ref.null $node global.set $root)

  (func $exception_root (local $exception exnref)
    block $outer (result (ref $node))
      try_table (catch $payload $outer)
        block $inner (result (ref $node) exnref)
          try_table (catch_ref $payload $inner)
            i32.const 505 call $make_selected throw $payload
          end
          unreachable
        end
        local.set $exception
        drop
        local.get $exception
        call $stress
        throw_ref
      end
      unreachable
    end
    struct.get $node 0
    i32.const 506 i32.ne if unreachable end)

  (func $spill_root {spill_locals}
{spill_initializers}
    call $stress
    i32.const 0
{spill_reads}
    i32.const {spill_expected} i32.ne if unreachable end)

  (func (export "_start")
{invocation}))
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--allocations", type=int, required=True)
    parser.add_argument("--warmup-calls", type=int, default=0,
                        help="indirect-call pressure before root checks; use to probe T2 entry")
    parser.add_argument("--root", choices=("all", *ROOTS), default="all")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if args.out.exists():
        parser.error("output path already exists")
    source = module(args.allocations, args.root, args.warmup_calls)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(source)
    manifest = {"allocations_per_root": args.allocations, "warmup_calls": args.warmup_calls,
                "root": args.root,
                "executed_roots": list(ROOTS if args.root == "all" else (args.root,)),
                "required_features": ["gc", "exceptions", "function-references"],
                "wat_sha256": hashlib.sha256(source.encode()).hexdigest()}
    args.out.with_suffix(".json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()

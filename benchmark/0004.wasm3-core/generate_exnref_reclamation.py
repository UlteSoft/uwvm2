#!/usr/bin/env python3
"""Generate self-checking dropped-exnref and exnref/GC-cycle release probes.

These cases intentionally discard all but the latest 1,024 aggregate roots.
They must run with a measured positive collection count before they can
qualify a future collector; a current monotonic store should fail the RSS gate.
"""

import argparse
from pathlib import Path


CASES = ("discarded-exnref", "exnref-aggregate-cycle")


def expected_after(count):
    value = 123456789
    for _ in range(count):
        value = (value * 1664525 + 1013904223) & 0xffffffff
    return value


def module(case, count):
    if case not in CASES or not 0 < count <= 0xffffffff:
        raise ValueError("unknown case or invalid iteration count")
    if case == "discarded-exnref":
        declarations = """
  (tag $payload (param i32))
  (func $thrower (param i32)
    local.get 0
    throw $payload)
  (func $step (param i32) (result i32)
    block $caught (result i32 (ref exn))
      try_table (catch_ref $payload $caught)
        local.get 0
        call $thrower
      end
      unreachable
    end
    drop)
"""
        invocation = "call $step"
        features = ("exceptions",)
    else:
        declarations = """
  (type $node (struct (field (mut (ref null exn))) (field i32)))
  (tag $payload (param (ref $node)))
  (table $roots (export "roots") 1024 (ref null $node))
  (func $step (param $index i32) (param $next i32) (result i32)
    (local $object (ref null $node))
    (local $exception (ref null exn))
    block $caught (result (ref $node) (ref exn))
      try_table (catch_ref $payload $caught)
        ref.null exn
        local.get $next
        struct.new $node
        throw $payload
      end
      unreachable
    end
    local.set $exception
    local.set $object
    local.get $object
    ref.as_non_null
    local.get $exception
    struct.set $node 0
    local.get $object
    ref.as_non_null
    struct.get $node 0
    ref.is_null
    if unreachable end
    local.get $index
    local.get $object
    table.set $roots
    local.get $index
    table.get $roots
    ref.as_non_null
    struct.get $node 1)
"""
        invocation = "call $step"
        features = ("exceptions", "gc", "function-references")
    index = "local.get $i\n      i32.const 1023\n      i32.and\n      " if case != "discarded-exnref" else ""
    text = f"""(module
{declarations}
  (func (export "_start") (local $i i32) (local $state i32)
    i32.const 123456789
    local.set $state
    block $done
      loop $again
        local.get $i
        i32.const {count}
        i32.ge_u
        br_if $done
        {index}local.get $state
        i32.const 1664525
        i32.mul
        i32.const 1013904223
        i32.add
        {invocation}
        local.set $state
        local.get $i
        i32.const 1
        i32.add
        local.set $i
        br $again
      end
    end
    local.get $state
    i32.const {expected_after(count)}
    i32.ne
    if unreachable end)
)
"""
    return text, features


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=CASES, required=True)
    parser.add_argument("--count", type=int, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    source, _ = module(args.case, args.count)
    args.out.write_text(source)
    print(f"{args.case}: {args.count} iterations, final LCG {expected_after(args.count)}")


if __name__ == "__main__":
    main()

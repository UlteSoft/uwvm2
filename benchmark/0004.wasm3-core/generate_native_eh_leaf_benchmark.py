#!/usr/bin/env python3
"""Prepare actual Core 3 WAT and an independent scalar contract, never execute it."""
import argparse
import hashlib
import json
from pathlib import Path

MASK = (1 << 32) - 1


def expected(n):
    if type(n) is not int or not 16 <= n < (1 << 31) or n % 16:
        raise ValueError("iterations must be a positive multiple of 16 below 2^31")
    # Compose affine LCG transforms. This is outside every guest timing region.
    result_a, result_b = 1, 0
    power_a, power_b = 1664525, 1013904223
    bits = n
    while bits:
        if bits & 1:
            result_a, result_b = (result_a * power_a) & MASK, (result_b * power_a + power_b) & MASK
        power_a, power_b = (power_a * power_a) & MASK, (power_b * (power_a + 1)) & MASK
        bits >>= 1
    # The low four bits visit all 16 states exactly once before repeating.
    # The leaf tests its input, starting at zero, so each complete cycle throws once.
    state, visited = 0, set()
    for _ in range(16):
        visited.add(state)
        state = (state * 1664525 + 1013904223) & 15
    if len(visited) != 16 or state != 0:
        raise AssertionError("independent low-bit cycle proof failed")
    return result_b, n // 16


def build(n, bad_checksum=False):
    checksum, catches = expected(n)
    checked = checksum ^ int(bad_checksum)
    wat = f'''(module
  (tag $event (param i32))
  (global $catches (export "catches") (mut i32) (i32.const 0))
  (global $checksum (export "checksum") (mut i32) (i32.const 0))
  (func $step (export "step") (param $x i32) (result i32)
    (local $next i32)
    local.get $x i32.const 1664525 i32.mul i32.const 1013904223 i32.add local.set $next
    local.get $x i32.const 15 i32.and i32.eqz
    if local.get $next throw $event end
    local.get $next)
  (func $run (export "run") (result i32)
    (local $index i32) (local $value i32)
    i32.const 0 global.set $catches
    i32.const 0 global.set $checksum
    block $done
      loop $again
        local.get $index i32.const {n} i32.ge_u br_if $done
        block $after (result i32)
          block $caught (result i32)
            try_table (result i32) (catch $event $caught)
              local.get $value call $step
            end
            br $after
          end
          global.get $catches i32.const 1 i32.add global.set $catches
        end
        local.set $value
        local.get $index i32.const 1 i32.add local.set $index
        br $again
      end
    end
    local.get $value global.set $checksum
    global.get $catches i32.const {catches} i32.ne if unreachable end
    local.get $value i32.const 0x{checked:08x} i32.ne if unreachable end
    local.get $value)
  (func (export "_start") call $run drop))
'''
    raw = wat.encode()
    manifest = dict(schema="uwvm-native-eh-leaf-benchmark-wat-v1", source_only=True,
                    iterations=n, expected_checksum_u32=checksum, expected_catches=catches,
                    leaf_local_index=0, entry_local_index=1, start_local_index=2,
                    input_seed_u32=0, throws_per_lowbit_cycle=1,
                    actual_wat_sha256=hashlib.sha256(raw).hexdigest(),
                    selfcheck_should_trap=bool(bad_checksum), expected_run_count=1,
                    actual_wasm_sha256=None, actual_official_validation=False,
                    actual_wasmtime_oracle=False, actual_product_execution=False,
                    private_publication_qualified=False, performance_qualified=False)
    return raw, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iterations", type=int, default=65536)
    parser.add_argument("--bad-checksum", action="store_true")
    parser.add_argument("--out-prefix", type=Path, required=True)
    args = parser.parse_args()
    wat, manifest = build(args.iterations, args.bad_checksum)
    # Exclusive creation never overwrites a previous fixture or receipt.
    args.out_prefix.parent.mkdir(parents=True, exist_ok=True)
    for suffix, raw in ((".wat", wat), (".json", (json.dumps(manifest, sort_keys=True, indent=2) + "\n").encode())):
        path = args.out_prefix.with_suffix(suffix)
        with path.open("xb") as stream:
            stream.write(raw)
    print(json.dumps(manifest, sort_keys=True))


if __name__ == "__main__":
    main()

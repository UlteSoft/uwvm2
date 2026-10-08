#!/usr/bin/env python3
"""Generate checked, source-identical Core 3 throughput fixtures.

The bytecode is shared by every VM. Each guest verifies its own final state,
so a successful process exit is a semantic prerequisite to a timing sample.
"""

import argparse
from functools import lru_cache
import hashlib
import json
from pathlib import Path


SEED = 123456789
A = 1664525
C = 1013904223
MASK = (1 << 32) - 1
CASES = ("gc-struct-update", "gc-array-update", "gc-struct-heap-update",
         "gc-array-heap-update", "gc-allocation-ring", "gc-cast",
         "gc-i31-box-unbox", "extended-const-init",
         "memory64-random-store", "memory64-high-random-store", "memory32-random-store",
         "memory32-page-aligned-store", "memory32-page-unaligned-store",
         "memory64-page-aligned-store", "memory64-page-unaligned-store",
         "memory64-atomic-rmw", "memory32-atomic-rmw",
         "memory64-atomic-wait-mismatch", "memory32-atomic-wait-mismatch",
         "memory64-atomic-notify-empty", "memory32-atomic-notify-empty",
         "multi-memory-random-store", "table64-indirect", "table32-indirect",
         "tail-call-step", "plain-call-step", "eh-caught-step", "eh-plain-step",
         "call-ref-step", "direct-call-step", "relaxed-swizzle", "strict-swizzle")


def endpoint(count):
    multiplier, increment = 1, 0
    power_multiplier, power_increment = A, C
    while count:
        if count & 1:
            multiplier = multiplier * power_multiplier & MASK
            increment = (increment * power_multiplier + power_increment) & MASK
        power_increment = (power_multiplier + 1) * power_increment & MASK
        power_multiplier = power_multiplier * power_multiplier & MASK
        count >>= 1
    return (multiplier * SEED + increment) & MASK


@lru_cache(maxsize=None)
def memory_endpoint(count):
    """Reference result for the exported, data-dependent random memory test."""
    cells = [0] * 4096
    state = SEED
    for _ in range(count):
        state = (state * A + C) & MASK
        cells[((state >> 2) & 16380) >> 2] = state
        state ^= cells[((state >> 10) & 16380) >> 2]
    # The guest reads this final location after the loop and checks the XOR.
    return state ^ cells[((state >> 2) & 16380) >> 2]


@lru_cache(maxsize=None)
def gc_heap_endpoint(count):
    """Reference state for alias-visible struct/array updates through a table."""
    cells = [0] * 1024
    state = SEED
    for _ in range(count):
        state = (state * A + C) & MASK
        index = (state >> 10) & 1023
        state ^= cells[index]
        cells[index] = state
    return state


def gc_cast_trace(count):
    """Count both dynamic heap-type paths without iterating millions of times.

    The LCG has full period 2^11 in its low eleven bits: its multiplier is
    one modulo four and its increment is odd. The selected table slot's type
    is the parity of bit ten, so each 2048-step period visits both types 1024
    times. Only a short remainder needs direct simulation.
    """
    periods, remainder = divmod(count, 2048)
    hits = [periods * 1024, periods * 1024]
    state = SEED
    for _ in range(remainder):
        state = (state * A + C) & MASK
        hits[(state >> 10) & 1] += 1
    return {"struct_lookups": hits[0], "array_lookups": hits[1]}


def table_index_trace(count):
    """Bounded witness that both distinct targets and all slots are visited."""
    sampled = min(count, 8192)
    state = SEED
    indices = set()
    targets = [0, 0]
    for _ in range(sampled):
        index = (state >> 10) & 1023
        indices.add(index)
        targets[index & 1] += 1
        state = (state * A + C + 2 * (index & 1)) & MASK
    return {"prefix_iterations": sampled, "distinct_indices_in_prefix": len(indices),
            "first_target_calls": targets[0], "second_target_calls": targets[1]}


@lru_cache(maxsize=None)
def table_indirect_endpoint(count):
    """Independent result for an actually mixed-target indirect-call table."""
    state = SEED
    for _ in range(count):
        index = (state >> 10) & 1023
        state = (state * A + C + 2 * (index & 1)) & MASK
    return state


def call_ref_trace(count):
    """Witness both targets of the exported typed-reference table are used."""
    sampled = min(count, 8192)
    state = SEED
    targets = [0, 0]
    for _ in range(sampled):
        selected = (state >> 10) & 1
        targets[selected] += 1
        state = (state * A + C + 2 * selected) & MASK
    return {"prefix_iterations": sampled, "first_target_calls": targets[0],
            "second_target_calls": targets[1]}


SWIZZLE_SIGN_PATTERN = 0xA65C


def swizzle_trace(count):
    """Bounded witness that the SIMD index and all-lane result both vary."""
    sampled = min(count, 8192)
    state = SEED
    indices = set()
    bitmasks = set()
    for _ in range(sampled):
        state = (state * A + C) & MASK
        shift = (state >> 16) & 15
        indices.add(shift)
        bits = ((SWIZZLE_SIGN_PATTERN >> shift) |
                (SWIZZLE_SIGN_PATTERN << (16 - shift))) & 0xffff
        if state & 0x80:
            bits ^= 0xffff
        bitmasks.add(bits)
        state ^= bits
    return {"prefix_iterations": sampled, "distinct_indices_in_prefix": len(indices),
            "distinct_bitmasks_in_prefix": len(bitmasks)}


@lru_cache(maxsize=None)
def swizzle_endpoint(count):
    """Independent scalar oracle for the full-vector swizzle feedback loop.

    Each lane's lookup index is (state_high_nibble + lane) modulo sixteen.
    `i8x16.bitmask` therefore rotates the chosen sign-bit pattern right by
    that nibble; a set sign bit in the splatted state inverts all sixteen
    output sign bits. The result feeds the next iteration's state.
    """
    state = SEED
    for _ in range(count):
        state = (state * A + C) & MASK
        shift = (state >> 16) & 15
        bits = ((SWIZZLE_SIGN_PATTERN >> shift) |
                (SWIZZLE_SIGN_PATTERN << (16 - shift))) & 0xffff
        if state & 0x80:
            bits ^= 0xffff
        state ^= bits
    return state


def expected_for_case(case, count):
    """One expectation function for both embedded checks and the manifest."""
    if "atomic-rmw" in case:
        return count
    if case == "gc-i31-box-unbox":
        return endpoint(count) & 0x7fffffff
    if case.endswith("random-store"):
        return memory_endpoint(count)
    if "-page-" in case:
        return page_memory_endpoint(count, "-aligned-" in case)
    if case in ("gc-struct-heap-update", "gc-array-heap-update"):
        return gc_heap_endpoint(count)
    if case in ("table32-indirect", "table64-indirect"):
        return table_indirect_endpoint(count)
    if case in ("call-ref-step", "direct-call-step"):
        return table_indirect_endpoint(count)
    if case in ("strict-swizzle", "relaxed-swizzle"):
        return swizzle_endpoint(count)
    return endpoint(count)


@lru_cache(maxsize=None)
def page_memory_trace(count, aligned):
    """Independent byte-stream result and actual 64 KiB boundary crossings."""
    bytes_ = bytearray(65539)
    mask = 65532 if aligned else 65535
    state = SEED
    crossing_stores = 0
    crossing_loads = 0
    for iteration in range(count):
        state = (state * A + C) & MASK
        # Every 256th iteration intentionally straddles the guest-page
        # boundary in the unaligned case. The aligned control performs the
        # same select, then masks that address down to the last full word.
        remaining = count - iteration
        write = (65534 if remaining & 255 == 0 else (state >> 2) & 65535) & mask
        crossing_stores += (write & 65535) > 65532
        bytes_[write:write + 4] = state.to_bytes(4, "little")
        read = (state >> 10) & mask
        crossing_loads += (read & 65535) > 65532
        state ^= int.from_bytes(bytes_[read:read + 4], "little")
    # The loop exits with $remaining == 0, so the same Wasm address
    # expression selects the boundary address for its final check.
    final = 65534 & mask
    crossing_loads += (final & 65535) > 65532
    expected = (state + int.from_bytes(bytes_[final:final + 4], "little")) & MASK
    return {"final_expected": expected, "crossing_stores": crossing_stores,
            "crossing_loads_including_final": crossing_loads}


def page_memory_endpoint(count, aligned):
    return page_memory_trace(count, aligned)["final_expected"]


def memory_footprint(count, base=0):
    """Count guest pages actually touched by the random memory workload.

    This describes Wasm addresses, not a host VM's committed physical pages.
    The latter must be measured separately by the bounded engine preflight.
    """
    cells = [0] * 4096
    pages_4k = set()
    pages_64k = set()
    minimum = None
    maximum = None
    state = SEED

    def touch(offset):
        nonlocal minimum, maximum
        address = base + offset
        pages_4k.add(address >> 12)
        pages_64k.add(address >> 16)
        minimum = address if minimum is None else min(minimum, address)
        maximum = address + 3 if maximum is None else max(maximum, address + 3)

    for _ in range(count):
        state = (state * A + C) & MASK
        write_offset = (state >> 2) & 16380
        touch(write_offset)
        cells[write_offset >> 2] = state
        read_offset = (state >> 10) & 16380
        touch(read_offset)
        state ^= cells[read_offset >> 2]
    touch((state >> 2) & 16380)
    return {"base_address": base, "minimum_address": minimum,
            "maximum_byte_address": maximum,
            "distinct_guest_4k_pages": len(pages_4k),
            "distinct_guest_64k_pages": len(pages_64k),
            "final_expected": state ^ cells[((state >> 2) & 16380) >> 2]}


STEP = "local.get $state i32.const 1664525 i32.mul i32.const 1013904223 i32.add local.set $state"


def module(case, count):
    if not 0 < count <= MASK:
        raise ValueError("iteration count must fit a positive i32")
    types = ""
    locals_ = ""
    initialization = ""
    operation = STEP
    final = "local.get $state"
    expected = expected_for_case(case, count)
    features = []
    state_init = f"i32.const {SEED}"
    if case == "gc-struct-update":
        features = ["gc"]
        types = "(type $s (struct (field (mut i32))))"
        locals_ = "(local $object (ref null $s))"
        initialization = "local.get $state struct.new $s local.set $object"
        operation = f"""local.get $object
          local.get $object struct.get $s 0
          i32.const {A} i32.mul i32.const {C} i32.add
          struct.set $s 0"""
        final = "local.get $object struct.get $s 0"
    elif case == "gc-array-update":
        features = ["gc"]
        types = "(type $a (array (mut i32)))"
        locals_ = "(local $object (ref null $a))"
        initialization = "local.get $state i32.const 1 array.new $a local.set $object"
        operation = f"""local.get $object i32.const 0
          local.get $object i32.const 0 array.get $a
          i32.const {A} i32.mul i32.const {C} i32.add
          array.set $a"""
        final = "local.get $object i32.const 0 array.get $a"
    elif case in ("gc-struct-heap-update", "gc-array-heap-update"):
        # A dynamic table index and state-dependent field read force a real
        # aggregate access on every iteration. Each table slot owns a distinct
        # aggregate, and the exported table keeps final heap state observable.
        # This complements the simple local-reference case, which an optimizer
        # may scalar-replace across the entire loop.
        features = ["gc"]
        struct = case == "gc-struct-heap-update"
        aggregate = "$s" if struct else "$a"
        declaration = "(type $s (struct (field (mut i32))))" if struct else "(type $a (array (mut i32)))"
        types = declaration + ' (table (export "roots") 1024 anyref)'
        locals_ = f"(local $object (ref null {aggregate})) (local $init i32)"
        create = "i32.const 0 struct.new $s" if struct else "i32.const 0 i32.const 1 array.new $a"
        initialization = f"""i32.const 0 local.set $init
          loop $initialize
            local.get $init {create} table.set 0
            local.get $init i32.const 1 i32.add local.tee $init
            i32.const 1024 i32.lt_u br_if $initialize
          end"""
        get = "struct.get $s 0" if struct else "i32.const 0 array.get $a"
        # struct.set requires the state after the object; array.set requires
        # object/index/state, so spell each stack form explicitly.
        update = ("local.get $object local.get $state struct.set $s 0" if struct else
                  "local.get $object i32.const 0 local.get $state array.set $a")
        operation = f"""{STEP}
          local.get $state i32.const 10 i32.shr_u i32.const 1023 i32.and
          table.get 0 ref.cast (ref {aggregate}) local.set $object
          local.get $object {get} local.get $state i32.xor local.set $state
          {update}"""
    elif case == "gc-allocation-ring":
        # Overwriting 1024 table roots makes older objects unreachable. This
        # measures allocation, reference barriers and eventual collection, not
        # a synthetic allocation whose result the compiler may discard.
        features = ["gc"]
        # Exporting the table makes replacement of its roots observable to a
        # host, so whole-module optimization cannot erase the allocations.
        types = '(type $s (struct (field i32))) (table (export "roots") 1024 anyref)'
        operation = f"""{STEP}
          local.get $remaining i32.const 1023 i32.and
          local.get $state struct.new $s table.set 0
          local.get $remaining i32.const 1023 i32.and
          table.get 0 ref.cast (ref $s) struct.get $s 0 local.set $state"""
    elif case == "gc-cast":
        features = ["gc"]
        # A state-dependent lookup into two genuinely different aggregate
        # heap types prevents a fixed always-true type test from describing
        # the workload. Export the table so its contents remain observable.
        types = ('(type $s (struct (field i32))) '
                 '(type $a (array i32)) (table (export "roots") 1024 anyref)')
        locals_ = "(local $init i32) (local $index i32)"
        initialization = """i32.const 0 local.set $init
          loop $initialize
            local.get $init
            local.get $init i32.const 1 i32.and
            if (result anyref)
              i32.const 0 i32.const 1 array.new $a
            else
              local.get $init struct.new $s
            end
            table.set 0
            local.get $init i32.const 1 i32.add local.tee $init
            i32.const 1024 i32.lt_u br_if $initialize
          end"""
        operation = f"""{STEP}
          local.get $state i32.const 10 i32.shr_u i32.const 1023 i32.and
          local.set $index
          local.get $index table.get 0 ref.test (ref $s)
          local.get $index i32.const 1 i32.and i32.eqz
          i32.ne if unreachable end"""
    elif case == "gc-i31-box-unbox":
        features = ["gc"]
        operation = f"{STEP} local.get $state ref.i31 i31.get_u local.set $state"
    elif case == "extended-const-init":
        features = ["extended-const"]
        # An immutable, module-defined global.get in a constant expression
        # additionally requires GC; use arithmetic alone to isolate the
        # extended-const proposal from that separate GC extension.
        types = f"(global $seed i32 (i32.add (i32.const {SEED}) (i32.const 0)))"
        state_init = "global.get $seed"
    elif case in ("memory64-random-store", "memory64-high-random-store",
                  "memory32-random-store"):
        wide = case.startswith("memory64")
        high = case == "memory64-high-random-store"
        features = ["memory64"] if wide else []
        # Exported memory makes the entire stream of writes observable after
        # invocation. The second, different address supplies a real data
        # dependency; an immediate same-address reload can be forwarded away.
        types = ('(memory (export "memory") i64 65537 65537)' if high else
                 '(memory (export "memory") i64 2 2)' if wide else
                 '(memory (export "memory") 2 2)')
        address = ("local.get $state i32.const 2 i32.shr_u i32.const 16380 i32.and"
                   + (" i64.extend_i32_u" if wide else ""))
        read_address = ("local.get $state i32.const 10 i32.shr_u i32.const 16380 i32.and"
                        + (" i64.extend_i32_u" if wide else ""))
        if high:
            address = "i64.const 4294967296 " + address + " i64.add"
            read_address = "i64.const 4294967296 " + read_address + " i64.add"
        operation = f"""{STEP}
          {address}
          local.get $state i32.store
          {read_address}
          i32.load local.get $state i32.xor local.set $state"""
        final = f"{address} i32.load local.get $state i32.xor"
    elif case in ("memory32-page-aligned-store", "memory32-page-unaligned-store",
                  "memory64-page-aligned-store", "memory64-page-unaligned-store"):
        wide = case.startswith("memory64")
        aligned = "-aligned-" in case
        mask = 65532 if aligned else 65535
        features = ["memory64"] if wide else []
        types = ('(memory (export "memory") i64 2 2)' if wide else
                 '(memory (export "memory") 2 2)')
        address = f"""i32.const 65534
          local.get $state i32.const 2 i32.shr_u i32.const 65535 i32.and
          local.get $remaining i32.const 255 i32.and i32.eqz
          select i32.const {mask} i32.and"""
        read_address = f"local.get $state i32.const 10 i32.shr_u i32.const {mask} i32.and"
        if wide:
            address += " i64.extend_i32_u"
            read_address += " i64.extend_i32_u"
        operation = f"""{STEP}
          {address} local.get $state i32.store align=1
          {read_address} i32.load align=1
          local.get $state i32.xor local.set $state"""
        final = f"{address} i32.load align=1 local.get $state i32.add"
    elif case in ("memory64-atomic-rmw", "memory32-atomic-rmw"):
        wide = case.startswith("memory64")
        features = ["memory64", "threads"] if wide else ["threads"]
        types = "(memory i64 1 1 shared)" if wide else "(memory 1 1 shared)"
        address = "i64.const 0" if wide else "i32.const 0"
        operation = f"{address} i32.const 1 i32.atomic.rmw.add drop"
        final = f"{address} i32.atomic.load"
    elif case in ("memory64-atomic-wait-mismatch", "memory32-atomic-wait-mismatch",
                  "memory64-atomic-notify-empty", "memory32-atomic-notify-empty"):
        wide = case.startswith("memory64")
        wait = "wait-mismatch" in case
        features = ["memory64", "threads"] if wide else ["threads"]
        types = "(memory i64 1 1 shared)" if wide else "(memory 1 1 shared)"
        address = "i64.const 0" if wide else "i32.const 0"
        # The shared cell starts at zero and is never changed. wait32 expects
        # one and therefore returns `not-equal` (1) without parking; notify
        # finds no waiter and returns zero. These are explicitly fast-path
        # instruction costs, never a two-thread wake-latency measurement.
        if wait:
            operation = f"""{STEP}
              {address} i32.const 1 i64.const -1 memory.atomic.wait32
              i32.const 1 i32.ne if unreachable end"""
        else:
            operation = f"""{STEP}
              {address} i32.const 1 memory.atomic.notify
              i32.eqz if else unreachable end"""
    elif case == "multi-memory-random-store":
        features = ["multi-memory"]
        types = '(memory $first 2 2) (memory $second (export "memory") 2 2)'
        address = "local.get $state i32.const 2 i32.shr_u i32.const 16380 i32.and"
        read_address = "local.get $state i32.const 10 i32.shr_u i32.const 16380 i32.and"
        operation = f"""{STEP}
          {address} local.get $state i32.store $second
          {read_address} i32.load $second
          local.get $state i32.xor local.set $state"""
        final = f"{address} i32.load $second local.get $state i32.xor"
    elif case in ("table64-indirect", "table32-indirect"):
        wide = case.startswith("table64")
        features = ["table64"] if wide else []
        # A constant slot zero or 1024 identical targets can become a direct
        # call. Alternate two distinct functions and select a slot from the
        # evolving state; the return value depends on the selected target.
        table = ('(table (export "targets") i64 1024 funcref)' if wide else
                 '(table (export "targets") 1024 funcref)')
        offset = "i64.const 0" if wide else "i32.const 0"
        element = f'(elem ({offset}) func ' + ' '.join(
            ('$step_a' if index & 1 == 0 else '$step_b') for index in range(1024)) + ')'
        types = f"""(type $signature (func (param i32) (result i32)))
          (func $step_a (type $signature) (param i32) (result i32)
            local.get 0 i32.const {A} i32.mul i32.const {C} i32.add)
          (func $step_b (type $signature) (param i32) (result i32)
            local.get 0 i32.const {A} i32.mul i32.const {C + 2} i32.add)
          {table} {element}"""
        index = ("local.get $state i32.const 10 i32.shr_u i32.const 1023 i32.and"
                 + (" i64.extend_i32_u" if wide else ""))
        operation = f"local.get $state {index} call_indirect (type $signature) local.set $state"
    elif case in ("tail-call-step", "plain-call-step", "call-ref-step", "direct-call-step"):
        typed = case in ("call-ref-step", "direct-call-step")
        tail = case == "tail-call-step"
        features = ["function-references"] if typed else (["tail-call"] if tail else [])
        if typed:
            types = f"""(type $signature (func (param i32) (result i32)))
              (func $step_a (type $signature) (param i32) (result i32)
                local.get 0 i32.const {A} i32.mul i32.const {C} i32.add)
              (func $step_b (type $signature) (param i32) (result i32)
                local.get 0 i32.const {A} i32.mul i32.const {C + 2} i32.add)"""
        else:
            types = f"""(type $signature (func (param i32) (result i32)))
              (func $step (type $signature) (param i32) (result i32)
                local.get 0 i32.const {A} i32.mul i32.const {C} i32.add)"""
        if case == "call-ref-step":
            # The table is exported and mutable by the embedder; its runtime
            # contents cannot be inferred from two ref.func literals in the
            # hot loop. Both distinct targets affect the evolving checksum.
            types += """(table $targets (export "targets") 2 (ref null $signature))
              (elem (i32.const 0) (ref null $signature) (ref.func $step_a) (ref.func $step_b))"""
            operation = ("local.get $state "
                         "local.get $state i32.const 10 i32.shr_u i32.const 1 i32.and "
                         "table.get $targets ref.as_non_null call_ref $signature local.set $state")
        elif case == "direct-call-step":
            # Match the two-target call_ref checksum and argument work. The
            # branch chooses a direct call, rather than fetching a typed ref.
            operation = ("local.get $state i32.const 10 i32.shr_u i32.const 1 i32.and "
                         "if (result i32) local.get $state call $step_b "
                         "else local.get $state call $step_a end local.set $state")
        elif not typed:
            types += " (func $wrapper (type $signature) (param i32) (result i32) local.get 0 " + (
                "return_call $step" if tail else "call $step") + ")"
            operation = "local.get $state call $wrapper local.set $state"
    elif case in ("eh-caught-step", "eh-plain-step"):
        caught = case == "eh-caught-step"
        features = ["exceptions"] if caught else []
        if caught:
            types = "(tag $event (param i32))"
            operation = f"""block $caught (result i32)
              try_table (catch $event $caught)
                {STEP}
                local.get $state throw $event
              end unreachable
            end local.set $state"""
        else:
            operation = STEP
    elif case in ("relaxed-swizzle", "strict-swizzle"):
        relaxed = case == "relaxed-swizzle"
        features = ["simd", "relaxed-simd"] if relaxed else ["simd"]
        swizzle = "i8x16.relaxed_swizzle" if relaxed else "i8x16.swizzle"
        signs = " ".join("-128" if SWIZZLE_SIGN_PATTERN & (1 << lane) else "0"
                         for lane in range(16))
        lanes = " ".join(str(lane) for lane in range(16))
        mask = " ".join("15" for _ in range(16))
        # A uniform input plus constant identity indices lets the optimizer
        # replace the entire swizzle with a scalar byte comparison. The
        # state-dependent in-range indices below use every output lane via
        # bitmask, and that mask feeds the next iteration's state.
        operation = f"""{STEP}
          local.get $state i8x16.splat
          v128.const i8x16 {signs} v128.xor
          local.get $state i32.const 16 i32.shr_u i32.const 15 i32.and i8x16.splat
          v128.const i8x16 {lanes} i8x16.add
          v128.const i8x16 {mask} v128.and
          {swizzle} i8x16.bitmask
          local.get $state i32.xor local.set $state"""
    else:
        raise ValueError(case)
    source = f"""(module
      {types}
      (func (export \"_start\") (local $state i32) (local $remaining i32) {locals_}
        {state_init} local.set $state
        i32.const {count} local.set $remaining
        {initialization}
        loop $again
          {operation}
          local.get $remaining i32.const 1 i32.sub local.tee $remaining br_if $again
        end
        {final} i32.const {expected} i32.ne if unreachable end))\n"""
    return source, features


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--count", type=int, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for case in CASES:
        source, features = module(case, args.count)
        path = args.out / f"{case}-{args.count}.wat"
        path.write_text(source)
        manifest[case] = {"wat": path.name, "sha256": hashlib.sha256(source.encode()).hexdigest(),
                          "features": features, "iterations": args.count,
                          "expected": expected_for_case(case, args.count)}
        if "-page-" in case:
            manifest[case]["boundary_trace"] = page_memory_trace(
                args.count, "-aligned-" in case)
        if case in ("strict-swizzle", "relaxed-swizzle"):
            manifest[case]["swizzle_trace"] = swizzle_trace(args.count)
        if case in ("call-ref-step", "direct-call-step"):
            manifest[case]["call_ref_trace"] = call_ref_trace(args.count)
    (args.out / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()

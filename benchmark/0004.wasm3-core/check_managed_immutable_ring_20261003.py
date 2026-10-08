#!/usr/bin/env python3
"""Check a managed ring's copied JSON receipt; do not launch or authenticate a VM."""
import argparse
import json
from pathlib import Path

SEED = 123456789
A = 1664525
C = 1013904223
MASK = (1 << 32) - 1
ROOTS = 1024


def require(condition, message):
    if not condition:
        raise ValueError(message)


def endpoint(count):
    require(type(count) is int and 0 <= count <= (1 << 31) - 1,
            "count must fit the existing signed language-loop carrier")
    mul, add, power_mul, power_add = 1, 0, A, C
    remaining = count
    while remaining:
        if remaining & 1:
            mul = mul * power_mul & MASK
            add = (add * power_mul + power_add) & MASK
        power_add = power_add * (power_mul + 1) & MASK
        power_mul = power_mul * power_mul & MASK
        remaining >>= 1
    return (mul * SEED + add) & MASK


def oracle(count):
    require(type(count) is int and ROOTS <= count <= (1 << 31) - 1,
            "all 1024 measured root slots must have been assigned")
    state = endpoint(count - ROOTS) if count > ROOTS else SEED
    slots = [None] * ROOTS
    for remaining in range(ROOTS, 0, -1):
        state = (state * A + C) & MASK
        slots[remaining & (ROOTS - 1)] = state
    require(state == endpoint(count) and all(type(x) is int for x in slots),
            "independent last-root construction")
    checksum = 0
    for value in slots:
        checksum ^= value
    return {"iterations": count, "checksum": state,
            "root_checksum": checksum, "root_slots": ROOTS}


def integer(payload, key, low=0, high=(1 << 63) - 1):
    value = payload[key]
    require(type(value) is int and low <= value <= high, "integer field: " + key)
    return value


def semantic_receipt(payload, *, count, runtime):
    require(type(payload) is dict and runtime in ("java", "dotnet"), "receipt/runtime")
    common = {"runtime", "iterations", "guest_ns", "checksum", "root_checksum"}
    java_fields = {"gc_collection_count", "gc_total_collection_time_ms",
                   "main_thread_allocated_bytes"}
    dotnet_fields = {"server_gc", "gc_heap_limit_config_bytes", "gc_heap_budget_bytes",
                     "gc_gen0_count", "gc_gen1_count", "gc_gen2_count",
                     "gc_total_pause_ns", "gc_allocated_bytes"}
    require(set(payload) == common | (java_fields if runtime == "java" else dotnet_fields),
            "exact existing telemetry-enabled producer fields")
    expected = oracle(count)
    require(payload["runtime"] == runtime, "runtime label")
    for key in ("iterations", "checksum", "root_checksum"):
        integer(payload, key, 0, MASK)
        require(payload[key] == expected[key], "semantic mismatch: " + key)
    internal_ns = integer(payload, "guest_ns", 1)
    if runtime == "java":
        collections = integer(payload, "gc_collection_count")
        allocated = integer(payload, "main_thread_allocated_bytes")
        pause = integer(payload, "gc_total_collection_time_ms") * 1_000_000
        heap = None
    else:
        require(type(payload["server_gc"]) is bool and not payload["server_gc"],
                "explicit workstation collector profile")
        heap = integer(payload, "gc_heap_limit_config_bytes", 1)
        require(heap == 1 << 30, "actual configured 1-GiB managed heap bound")
        integer(payload, "gc_heap_budget_bytes", 1, 1 << 30)
        collections = integer(payload, "gc_gen0_count")
        integer(payload, "gc_gen1_count")
        integer(payload, "gc_gen2_count")
        allocated = integer(payload, "gc_allocated_bytes")
        pause = integer(payload, "gc_total_pause_ns")
    # Nonzero telemetry does not establish an exact dynamic object count or the
    # native instruction path. Those require the actual layout/JIT evidence.
    # A source-equivalent language run also does not authenticate any producer,
    # source tree, executable, runtime DSO, scheduler admission, or performance.
    return {"schema": "uwvm-managed-immutable-ring-semantic-receipt-v1",
            "semantic_passed": True, "iterations": count,
            "checksum_u32": expected["checksum"],
            "root_checksum_u32": expected["root_checksum"],
            "root_slots": ROOTS, "syntactic_allocations": count,
            "dynamic_allocation_count": None,
            "reported_allocated_bytes": allocated,
            "reported_collection_count": collections,
            "reported_collection_time_ns": pause,
            "reported_internal_loop_ns": internal_ns,
            "managed_heap_config_bytes": heap,
            "observed_reclaiming_collector_activity": allocated > 0 and collections > 0,
            "same_wasm_bytes": False, "producer_authenticated": False,
            "counter_qualified": False, "performance_passed": False,
            "formal_acceptance": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipt", required=True, type=Path)
    parser.add_argument("--runtime", required=True, choices=("java", "dotnet"))
    parser.add_argument("--count", type=int, default=16_000_000)
    args = parser.parse_args()
    payload = json.loads(args.receipt.read_text())
    print(json.dumps(semantic_receipt(payload, count=args.count, runtime=args.runtime),
                     sort_keys=True))


if __name__ == "__main__":
    main()

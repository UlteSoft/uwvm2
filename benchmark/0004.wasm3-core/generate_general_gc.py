#!/usr/bin/env python3
"""Emit small, self-checking Core 3 GC binaries and matching readable WAT.

No external compiler or VM is run. Binary/WAT agreement and VM correctness
still require the keeper's official wasm-tools and Wasmtime cold checks.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile

FAMILIES = ("mutable-struct", "reference-cycle", "numeric-array", "reference-array")
PHASES = ("allocate", "mutate")
MASK = 0xFFFFFFFF
SEED = 123456789
ROOTS = 1024
WIDTH = 8
SALT = 0xA5C31F27


def uleb(value):
    if type(value) is not int or value < 0 or value > MASK:
        raise ValueError("u32 out of range")
    out = bytearray()
    while True:
        byte = value & 127
        value >>= 7
        out.append(byte | (128 if value else 0))
        if not value:
            return bytes(out)


def sleb(value):
    out = bytearray()
    while True:
        byte = value & 127
        value >>= 7
        done = (value == 0 and byte < 64) or (value == -1 and byte >= 64)
        out.append(byte | (0 if done else 128))
        if done:
            return bytes(out)


def signed32(value):
    value &= MASK
    return value if value < 0x80000000 else value - 0x100000000


def vector(items):
    return uleb(len(items)) + b"".join(items)


def name(value):
    data = value.encode("utf-8")
    return uleb(len(data)) + data


def section(ident, data):
    return bytes([ident]) + uleb(len(data)) + data


def check_parameters(family, phase, iterations):
    if family not in FAMILIES or phase not in PHASES:
        raise ValueError("unknown family/phase")
    # This is deliberately a small first-round generator, not a 512M oracle.
    if type(iterations) is not int or not ROOTS <= iterations <= 2_000_000:
        raise ValueError("iterations must be in [1024, 2000000]")


def oracle(family, phase, iterations):
    """Independent scalar object/array model, never an instruction interpreter."""
    check_parameters(family, phase, iterations)
    ring = [None] * ROOTS
    state = SEED
    total = 0

    def fresh(value):
        if family == "mutable-struct":
            return {"a": value}
        if family == "numeric-array":
            return {"cells": [value] * WIDTH}
        return {"a": value, "b": (value * 3 + 17) & MASK,
                "cells": [0] * WIDTH}  # 0=A, 1=B, both form a two-node cycle.

    def retained(obj):
        if family == "mutable-struct":
            return obj["a"]
        if family == "reference-cycle":
            return (obj["a"] + obj["b"]) & MASK
        if family == "numeric-array":
            return sum(obj["cells"]) & MASK
        return sum(obj["b"] if slot else obj["a"] for slot in obj["cells"]) & MASK

    if phase == "mutate":
        for i in range(ROOTS):
            state = (state * 1664525 + 1013904223) & MASK
            ring[i] = fresh(state)
    for i in range(iterations):
        state = (state * 1664525 + 1013904223) & MASK
        slot = i & (ROOTS - 1)
        if phase == "allocate":
            if i >= ROOTS:
                total = (total + retained(ring[slot])) & MASK
            ring[slot] = fresh(state)
        obj = ring[slot]
        value = (state + i) & MASK
        if family == "mutable-struct":
            obj["a"] = value
            observed = obj["a"]
        elif family == "reference-cycle":
            obj["a"] = value
            # A.next=A, observe A through that edge; restore A.next=B,
            # observe B, and assert B.next=A in the generated guest.
            observed = (2 * obj["a"] + obj["b"]) & MASK
        elif family == "numeric-array":
            selected = i & (WIDTH - 1)
            following = (selected + 1) & (WIDTH - 1)
            obj["cells"][selected] = state ^ SALT
            obj["cells"][following] = value
            observed = (obj["cells"][selected] + obj["cells"][following] + WIDTH) & MASK
        else:
            obj["a"] = value
            selected = i & (WIDTH - 1)
            following = (selected + 1) & (WIDTH - 1)
            obj["cells"][selected] = 1
            obj["cells"][following] = 0
            observed = (obj["b"] + obj["a"] + WIDTH) & MASK
        total = (total + observed) & MASK
    rootsum = sum(map(retained, ring)) & MASK
    per_created = {"mutable-struct": 1, "reference-cycle": 2,
                   "numeric-array": 1, "reference-array": 3}[family]
    created_groups = iterations if phase == "allocate" else ROOTS
    return {"step_checksum_u32": total, "root_checksum_u32": rootsum,
            "return_checksum_u32": total ^ rootsum, "last_lcg_u32": state,
            "guest_planned_allocations": per_created * created_groups,
            "allocation_groups": created_groups,
            "overwritten_root_groups": max(0, iterations - ROOTS) if phase == "allocate" else 0,
            "final_root_group_count": ROOTS,
            "final_reachable_objects": per_created * ROOTS,
            "guest_main_iterations": iterations,
            "guest_main_allocations": per_created * iterations if phase == "allocate" else 0,
            "collection_count": None, "reclaimed_count": None,
            "count_contract": "planned syntax operations; actual GC/reclaimed counters must be measured"}


class Code:
    def __init__(self):
        self.binary = bytearray()
        self.wat = []
        self.static_opcodes = set()

    def emit(self, text, data):
        self.wat.append("    " + text)
        self.binary.extend(data)
        self.static_opcodes.add(text.split()[0])

    def i32(self, value):
        value = signed32(value)
        self.emit(f"i32.const {value}", b"\x41" + sleb(value))

    def get(self, index):
        self.emit(f"local.get {index}", b"\x20" + uleb(index))

    def set(self, index):
        self.emit(f"local.set {index}", b"\x21" + uleb(index))

    def op(self, text, opcode):
        self.emit(text, bytes([opcode]))

    def gc(self, text, opcode, *immediates):
        self.emit(text, b"\xfb" + uleb(opcode) + b"".join(uleb(i) for i in immediates))

    def struct_get(self, field, local=6):
        self.get(local)
        self.gc(f"struct.get 0 {field}", 2, 0, field)

    def struct_set(self, field, value_local, local=6):
        self.get(local)
        self.get(value_local)
        self.gc(f"struct.set 0 {field}", 5, 0, field)

    def trap_unless_equal(self):
        self.op("i32.ne", 0x47)
        self.emit("if", b"\x04\x40")
        self.op("unreachable", 0)
        self.op("end", 0x0B)


def build(family, phase, iterations):
    expected = oracle(family, phase, iterations)
    refs = family in ("reference-cycle", "reference-array")
    array = family in ("numeric-array", "reference-array")
    array_type = 1 if family == "reference-array" else 0
    if refs:
        # Explicit unary recursive group for the self-referencing node.
        types = [b"\x4e\x01\x5f\x02\x7f\x01\x63\x00\x01"]
        type_wat = ["(rec (type (struct (field (mut i32)) (field (mut (ref null 0))))))"]
        if array:
            types.append(b"\x5e\x63\x00\x01")
            type_wat.append("(type (array (mut (ref null 0))))")
    elif array:
        types = [b"\x5e\x7f\x01"]
        type_wat = ["(type (array (mut i32)))"]
    else:
        types = [b"\x5f\x01\x7f\x01"]
        type_wat = ["(type (struct (field (mut i32))))"]
    run_type = len(types)
    types += [b"\x60\x00\x01\x7f", b"\x60\x00\x00"]
    type_wat += ["(type (func (result i32)))", "(type (func))"]
    c = Code()

    def lcg():
        c.get(1); c.i32(1664525); c.op("i32.mul", 0x6C)
        c.i32(1013904223); c.op("i32.add", 0x6A); c.set(1)

    def slot():
        c.get(0); c.i32(ROOTS - 1); c.op("i32.and", 0x71); c.set(3)

    def load_root():
        c.get(3); c.emit("table.get 0", b"\x25\x00")
        heap = array_type if array else 0
        c.gc(f"ref.cast (ref {heap})", 22, heap); c.set(8 if array else 6)
        if family == "reference-array":
            # Slot 0 is not necessarily A after mutation; find A through a
            # retained, trusted cycle by following B.next if needed. Instead
            # root A in array slot 7 is invariant only before mutation, so use
            # an exported second table that roots A independently (below).
            c.get(3); c.emit("table.get 1", b"\x25\x01")
            c.gc("ref.cast (ref 0)", 22, 0); c.set(6)
        if refs:
            c.struct_get(1); c.op("ref.as_non_null", 0xD4); c.set(7)

    def create():
        c.get(1)
        if refs:
            c.emit("ref.null 0", b"\xd0\x00")
            c.gc("struct.new 0", 0, 0); c.set(6)
            c.get(1); c.i32(3); c.op("i32.mul", 0x6C)
            c.i32(17); c.op("i32.add", 0x6A); c.get(6)
            c.gc("struct.new 0", 0, 0); c.set(7)
            c.struct_set(1, 7)
            if array:
                c.get(6); c.i32(WIDTH)
                c.gc("array.new 1", 6, 1); c.set(8)
        elif array:
            c.i32(WIDTH); c.gc("array.new 0", 6, 0); c.set(8)
        else:
            c.gc("struct.new 0", 0, 0); c.set(6)
        c.get(3); c.get(8 if array else 6)
        c.emit("table.set 0", b"\x26\x00")
        if family == "reference-array":
            c.get(3); c.get(6); c.emit("table.set 1", b"\x26\x01")

    def cycle_check():
        if refs:
            c.get(7); c.gc("struct.get 0 1", 2, 0, 1)
            c.get(6); c.op("ref.eq", 0xD3)
            c.i32(1); c.trap_unless_equal()

    def retained():
        # Leave one actual readback-derived i32 on the operand stack.
        if not array:
            c.struct_get(0)
            if refs:
                c.struct_get(0, 7); c.op("i32.add", 0x6A)
        else:
            c.get(8); c.gc("array.len", 15)
            c.i32(WIDTH); c.trap_unless_equal()
            c.i32(0)
            for j in range(WIDTH):
                c.get(8); c.i32(j)
                c.gc(f"array.get {array_type}", 11, array_type)
                if refs:
                    c.gc("struct.get 0 0", 2, 0, 0)
                c.op("i32.add", 0x6A)
        cycle_check()

    def accumulate_retained():
        c.get(2); retained(); c.op("i32.add", 0x6A); c.set(2)

    def advance(limit):
        c.get(0); c.i32(1); c.op("i32.add", 0x6A); c.set(0)
        c.get(0); c.i32(limit); c.op("i32.lt_u", 0x49)
        c.emit("br_if 0", b"\x0d\x00"); c.op("end", 0x0B)

    c.i32(SEED); c.set(1)
    if phase == "mutate":
        c.emit("loop", b"\x03\x40")
        lcg(); slot(); create(); advance(ROOTS)
        c.i32(0); c.set(0)
    c.emit("loop", b"\x03\x40")
    lcg(); slot()
    if phase == "allocate":
        c.get(0); c.i32(ROOTS); c.op("i32.ge_u", 0x4F)
        c.emit("if", b"\x04\x40")
        load_root(); accumulate_retained(); c.op("end", 0x0B)
        create()
    else:
        load_root()
    c.get(1); c.get(0); c.op("i32.add", 0x6A); c.set(5)
    c.get(2)
    if family == "mutable-struct":
        c.struct_set(0, 5); c.struct_get(0)
    elif family == "reference-cycle":
        c.struct_set(0, 5)
        c.struct_set(1, 6)  # A.next=A; the next read must observe that write.
        c.struct_get(0)
        c.struct_get(1); c.gc("struct.get 0 0", 2, 0, 0)
        c.op("i32.add", 0x6A)
        c.struct_set(1, 7)  # Restore A.next=B before this independent read.
        c.struct_get(1); c.gc("struct.get 0 0", 2, 0, 0)
        c.op("i32.add", 0x6A); cycle_check()
    else:
        if refs:
            c.struct_set(0, 5)
        c.get(0); c.i32(WIDTH - 1); c.op("i32.and", 0x71); c.set(4)
        c.get(8); c.get(4)
        if refs:
            c.get(7)
        else:
            c.get(1); c.i32(SALT); c.op("i32.xor", 0x73)
        c.gc(f"array.set {array_type}", 14, array_type)
        c.get(8); c.get(4); c.i32(1); c.op("i32.add", 0x6A)
        c.i32(WIDTH - 1); c.op("i32.and", 0x71)
        c.get(6 if refs else 5); c.gc(f"array.set {array_type}", 14, array_type)
        c.get(8); c.get(4); c.gc(f"array.get {array_type}", 11, array_type)
        if refs:
            c.gc("struct.get 0 0", 2, 0, 0)
        c.get(8); c.get(4); c.i32(1); c.op("i32.add", 0x6A)
        c.i32(WIDTH - 1); c.op("i32.and", 0x71)
        c.gc(f"array.get {array_type}", 11, array_type)
        if refs:
            c.gc("struct.get 0 0", 2, 0, 0)
        c.op("i32.add", 0x6A)
        c.get(8); c.gc("array.len", 15); c.op("i32.add", 0x6A)
        cycle_check()
    c.op("i32.add", 0x6A); c.set(2)
    advance(iterations)
    c.get(2); c.emit("global.set 0", b"\x24\x00")
    c.i32(0); c.set(2); c.i32(0); c.set(0)
    c.emit("loop", b"\x03\x40")
    slot(); load_root(); accumulate_retained(); advance(ROOTS)
    c.get(2); c.emit("global.set 1", b"\x24\x01")
    c.get(1); c.i32(expected["last_lcg_u32"]); c.trap_unless_equal()
    for index, key in [(0, "step_checksum_u32"), (1, "root_checksum_u32")]:
        c.emit(f"global.get {index}", b"\x23" + uleb(index))
        c.i32(expected[key]); c.trap_unless_equal()
    c.emit("global.get 0", b"\x23\x00")
    c.emit("global.get 1", b"\x23\x01"); c.op("i32.xor", 0x73)
    c.op("end", 0x0B)
    wrapper = Code()
    wrapper.emit("call 0", b"\x10\x00")
    wrapper.i32(expected["return_checksum_u32"]); wrapper.trap_unless_equal()
    wrapper.op("end", 0x0B)
    local_types = [b"\x7f"] * 6 + [b"\x63\x00"] * 2 + [b"\x63" + sleb(array_type)]
    # Canonical adjacent local runs make a no-name WAT roundtrip byte-identical.
    local_groups = [uleb(6) + b"\x7f"]
    if array_type == 0:
        local_groups.append(uleb(3) + b"\x63\x00")
    else:
        local_groups += [uleb(2) + b"\x63\x00", uleb(1) + b"\x63\x01"]
    locals_binary = vector(local_groups)
    body = locals_binary + bytes(c.binary)
    wrapper_body = b"\x00" + bytes(wrapper.binary)
    table_count = 2 if family == "reference-array" else 1
    tables = [b"\x6e\x00" + uleb(ROOTS)] * table_count
    exports = [("run", 0, 0), ("_start", 0, 1), ("roots", 1, 0),
               ("checksum", 3, 0), ("root_checksum", 3, 1)]
    if table_count == 2:
        exports.append(("node_roots", 1, 1))
    binary = b"\x00asm\x01\x00\x00\x00" + b"".join([
        section(1, vector(types)), section(3, vector([uleb(run_type), uleb(run_type + 1)])),
        section(4, vector(tables)), section(6, vector([b"\x7f\x01\x41\x00\x0b"] * 2)),
        section(7, vector([name(n) + bytes([kind]) + uleb(index) for n, kind, index in exports])),
        section(10, vector([uleb(len(body)) + body, uleb(len(wrapper_body)) + wrapper_body]))])
    local_wat = " ".join("i32" if t == b"\x7f" else f"(ref null {t[-1]})" for t in local_types)
    wat = "(module\n  " + "\n  ".join(type_wat) + "\n"
    wat += "  (table 1024 anyref)\n"
    if table_count == 2:
        wat += "  (table 1024 anyref)\n"
    wat += '  (global (mut i32) (i32.const 0))\n'
    wat += '  (global (mut i32) (i32.const 0))\n'
    # The last binary end belongs to the function; WAT's close paren supplies it.
    wat += f'  (func (type {run_type}) (result i32) (local {local_wat})\n'
    wat += "\n".join(c.wat[:-1]) + ")\n"
    wat += f'  (func (type {run_type + 1})\n'
    wat += "\n".join(wrapper.wat[:-1]) + ")\n"
    # Inline exports follow declaration order (tables/globals before functions)
    # and do not preserve the already fixed binary section-7 order. Explicit
    # declarations preserve exact name/kind/index/order without changing any
    # instruction, local, scalar oracle, root or binary byte.
    for export_name, export_kind, export_index in exports:
        export_type = {0: "func", 1: "table", 3: "global"}[export_kind]
        wat += f'  (export "{export_name}" ({export_type} {export_index}))\n'
    wat += ")\n"
    manifest = {"schema": "uwvm-general-gc-fixture-v1", "family": family, "phase": phase,
                "iterations": iterations, "root_ring": ROOTS, "array_length": WIDTH if array else None,
                "expected": expected, "required_features": ["gc", "reference-types", "function-references"],
                "static_instruction_names": sorted(c.static_opcodes),
                "table_root_slots": ROOTS * table_count,
                "compact_numeric_eligible": False,
                "compact_reason": "mutable numeric field or reference-bearing aggregate; never immutable one-field compact admission",
                "allocation_region": "whole run; allocation phase or initial 1024-group setup for mutate phase",
                "collection_region": "automatic collector boundaries are not a separately timed guest ROI",
                "mutation_region": "main loop; overlaps reads and scalar arithmetic, not a pure store timer",
                "lookup_roots_region": "old-root readback before replacement and final complete ring traversal",
                "vm_validation": "pending remote official wasm-tools and exact-byte Wasmtime oracle",
                "wat_sha256": hashlib.sha256(wat.encode()).hexdigest(),
                "wasm_sha256": hashlib.sha256(binary).hexdigest(), "wasm_bytes": len(binary)}
    return binary, wat, manifest


def write_new(path, data):
    """Bounded atomic new file publication; never replace prior evidence."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix=".general-gc-", delete=False) as out:
        temp = Path(out.name)
        try:
            out.write(data)
            out.flush()
            os.fsync(out.fileno())
            os.link(temp, path)  # Existing destination fails rather than overwrites.
        finally:
            temp.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--family", choices=FAMILIES, required=True)
    parser.add_argument("--phase", choices=PHASES, required=True)
    parser.add_argument("--iterations", type=int, default=65536)
    parser.add_argument("--out-prefix", type=Path, required=True)
    args = parser.parse_args()
    files = [args.out_prefix.with_suffix(suffix) for suffix in (".wasm", ".wat", ".json")]
    if any(path.exists() for path in files):
        parser.error("output exists; choose a new prefix")
    binary, wat, manifest = build(args.family, args.phase, args.iterations)
    for path, data in zip(files, [binary, wat.encode(), (json.dumps(manifest, indent=2) + "\n").encode()]):
        write_new(path, data)
    print(json.dumps({"files": [str(path) for path in files], "wasm_sha256": manifest["wasm_sha256"],
                      "validation": "source-only; pending keeper VM oracle"}))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Pure binary-shape/math checks only; never executes Wasm or a native guard."""

import ast
import hashlib
import importlib.util
import json
import re
from pathlib import Path
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SOURCE = HERE / "generate_general_gc.py"
spec = importlib.util.spec_from_file_location("general_gc_source", SOURCE)
g = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g)


def read_uleb(data, offset):
    result = 0
    start = offset
    for shift in range(0, 35, 7):
        if offset >= len(data):
            raise ValueError("truncated LEB")
        byte = data[offset]
        offset += 1
        result |= (byte & 127) << shift
        if not byte & 128:
            if result > g.MASK or data[start:offset] != g.uleb(result):
                raise ValueError("overflow/noncanonical LEB")
            return result, offset
    raise ValueError("unterminated LEB")


def sections(binary):
    if binary[:8] != b"\x00asm\x01\x00\x00\x00":
        raise ValueError("bad module header")
    offset = 8
    records = []
    while offset != len(binary):
        ident = binary[offset]
        offset += 1
        size, offset = read_uleb(binary, offset)
        end = offset + size
        if end > len(binary):
            raise ValueError("section beyond module")
        records.append((ident, binary[offset:end]))
        offset = end
    if [key for key, _ in records] != [1, 3, 4, 6, 7, 10]:
        raise ValueError("unexpected section layout")
    return dict(records)


def export_records(data):
    count, offset = read_uleb(data, 0)
    records = []
    for _ in range(count):
        size, offset = read_uleb(data, offset)
        if size > len(data) - offset or offset + size == len(data):
            raise ValueError("truncated export name/kind")
        export_name = data[offset:offset + size].decode("utf-8")
        offset += size
        kind = data[offset]
        offset += 1
        index, offset = read_uleb(data, offset)
        records.append((export_name, kind, index))
    if offset != len(data):
        raise ValueError("extra export section bytes")
    return records


def scalar_reference(family, phase, n):
    """Separate flat-array/edge model, not shared with the generator oracle."""
    first = [0] * 1024
    second = [0] * 1024
    lanes = [[0] * 8 for _ in range(1024)]
    s = 123456789
    checksum = 0
    maximum = (1 << 32) - 1

    def new(index, value):
        first[index] = value
        second[index] = (value * 3 + 17) % (1 << 32)
        lanes[index] = [value] * 8 if family == "numeric-array" else [0] * 8

    def root_value(index):
        if family == "mutable-struct":
            return first[index]
        if family == "reference-cycle":
            return first[index] + second[index]
        if family == "numeric-array":
            return sum(lanes[index])
        return sum([first[index], second[index]][edge] for edge in lanes[index])

    if phase == "mutate":
        for index in range(1024):
            s = (1664525 * s + 1013904223) % (1 << 32)
            new(index, s)
    for step in range(n):
        s = (1664525 * s + 1013904223) % (1 << 32)
        r = step % 1024
        if phase == "allocate":
            if step >= 1024:
                checksum += root_value(r)
            new(r, s)
        if family == "mutable-struct":
            first[r] = (s + step) & maximum
            checksum += first[r]
        elif family == "reference-cycle":
            first[r] = (s + step) & maximum
            # Two explicitly observed edge targets A then B.
            checksum += first[r] + first[r] + second[r]
        else:
            left = step % 8
            right = (step + 1) % 8
            if family == "numeric-array":
                lanes[r][left] = s ^ 2781028135
                lanes[r][right] = (s + step) & maximum
                checksum += lanes[r][left] + lanes[r][right] + 8
            else:
                first[r] = (s + step) & maximum
                lanes[r][left] = 1
                lanes[r][right] = 0
                checksum += second[r] + first[r] + 8
        checksum %= (1 << 32)
    roots = sum(root_value(index) for index in range(1024)) % (1 << 32)
    return checksum, roots, checksum ^ roots, s


def main():
    ast.parse(SOURCE.read_text())
    positives = []
    negatives = []
    for family in g.FAMILIES:
        for phase in g.PHASES:
            for n in (1024, 1025, 2048):
                binary, wat, manifest = g.build(family, phase, n)
                expected = manifest["expected"]
                actual = tuple(expected[key] for key in ("step_checksum_u32", "root_checksum_u32",
                                                        "return_checksum_u32", "last_lcg_u32"))
                assert actual == scalar_reference(family, phase, n), (family, phase, n)
                payloads = sections(binary)
                assert len(binary) < 2048
                assert wat.count("(") == wat.count(")")
                declarations = re.findall(r'^  \(export "([^"]+)" \((func|table|global) ([0-9]+)\)\)$', wat, re.M)
                actual_exports = [(name, {"func": 0, "table": 1, "global": 3}[kind], int(index))
                                  for name, kind, index in declarations]
                expected_exports = [("run", 0, 0), ("_start", 0, 1), ("roots", 1, 0),
                                    ("checksum", 3, 0), ("root_checksum", 3, 1)]
                if family == "reference-array":
                    expected_exports.append(("node_roots", 1, 1))
                assert actual_exports == export_records(payloads[7]) == expected_exports
                assert 'global.get 0' in wat and 'global.get 1' in wat
                assert 'ref.cast (ref' in wat and 'unreachable' in wat
                assert manifest["compact_numeric_eligible"] is False
                assert expected["collection_count"] is None
                assert hashlib.sha256(binary).hexdigest() == manifest["wasm_sha256"]
                table_count, _ = read_uleb(payloads[4], 0)
                assert table_count == (2 if family == "reference-array" else 1)
                body_count, offset = read_uleb(payloads[10], 0)
                assert body_count == 2
                for _ in range(body_count):
                    body_size, offset = read_uleb(payloads[10], offset)
                    assert payloads[10][offset + body_size - 1] == 0x0B
                    offset += body_size
                assert offset == len(payloads[10])
                if "reference" in family:
                    assert "ref.eq" in wat and "(rec" in wat and "struct.set 0 1" in wat
                if "array" in family:
                    assert "array.len" in wat and "array.set" in wat and "array.get" in wat
                assert expected["guest_main_allocations"] == (0 if phase == "mutate" else expected["guest_planned_allocations"])
                positives.append({"family": family, "phase": phase, "n": n,
                                  "wasm_sha256": manifest["wasm_sha256"], "wasm_bytes": len(binary)})
    for value in (0, 1, 63, 64, 127, 128, 16383, 16384, 0xFFFFFFFF):
        encoded = g.uleb(value)
        assert read_uleb(encoded, 0) == (value, len(encoded))
    for invalid in (b"", b"\x80", b"\x80\x00", b"\xff\xff\xff\xff\x1f"):
        try:
            read_uleb(invalid, 0)
        except ValueError:
            negatives.append("LEB " + invalid.hex())
        else:
            raise AssertionError("accepted bad LEB")
    for family, phase, n in [("bad", "allocate", 1024), ("mutable-struct", "bad", 1024),
                             ("mutable-struct", "allocate", 1023), ("reference-cycle", "mutate", 2000001),
                             ("numeric-array", "allocate", True)]:
        try:
            g.build(family, phase, n)
        except ValueError:
            negatives.append(f"parameters {family}/{phase}/{n}")
        else:
            raise AssertionError("accepted bad parameters")
    binary, _, _ = g.build("mutable-struct", "allocate", 1024)
    for corrupt in (b"bad" + binary[3:], binary[:-1]):
        try:
            sections(corrupt)
        except ValueError:
            negatives.append("binary corrupt/truncated")
        else:
            raise AssertionError("accepted bad binary")
    print(json.dumps({"schema": "uwvm-general-gc-static-check-v1", "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                      "math_binary_shape_controls": len(positives), "negative_controls": len(negatives),
                      "controls": positives, "negative_reasons": negatives,
                      "source_only": True, "wasm_official_validation": False,
                      "wasm_vm_execution": False, "native_execution": False,
                      "performance_qualified": False}, indent=2))


if __name__ == "__main__":
    main()

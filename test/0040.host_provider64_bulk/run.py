#!/usr/bin/env python3
"""Finite real-host-provider memory64 tests; launch only inside keeper's CG."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def digest(data):
    return hashlib.sha256(data).hexdigest()


def command(argv, out, label):
    completed = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90)
    (out / (label + ".stdout")).write_bytes(completed.stdout)
    (out / (label + ".stderr")).write_bytes(completed.stderr)
    return completed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--wat2wasm", required=True)
    parser.add_argument("--component", action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    oracle = json.loads((here / "oracle.json").read_text())
    wat = here / "provider64.wat"
    assert digest(wat.read_bytes()) == oracle["wat_sha256"]
    args.output.mkdir(exist_ok=False, parents=True)
    wasm = args.output / "provider64.wasm"
    assembled = command([args.wat2wasm, "--enable-all", str(wat), "-o", str(wasm)], args.output, "wabt")
    assert assembled.returncode == 0, "official WABT assembly failed"
    assert wasm.is_file() and 0 < wasm.stat().st_size <= 65536
    seeded = bytes((index * 37 + 11) & 255 for index in range(65536))
    rows = []
    for product, component in enumerate(args.component):
        binary = Path(component).resolve(strict=True)
        binary_pin = {"path": str(binary), "size": binary.stat().st_size,
                      "sha256": digest(binary.read_bytes())}
        for case in oracle["cases"]:
            label = f"product{product}-function{case['function']}"
            backing = args.output / (label + ".memory")
            backing.write_bytes(seeded)
            completed = command([str(binary), str(wasm), str(backing), str(case["function"])], args.output, label)
            receipts = []
            for line in completed.stdout.splitlines():
                try:
                    item = json.loads(line)
                except (json.JSONDecodeError, UnicodeDecodeError):
                    continue
                if item.get("schema") == "uwvm.host-provider64-bulk.actual-function.v1":
                    receipts.append(item)
            expected_receipt = {
                "schema": "uwvm.host-provider64-bulk.actual-function.v1", "phase": "pre-call",
                "function": case["function"], "host_provider": True, "memory_address_bits": 64,
                "analyzed_ir_both_bridges": True, "runtime_mode": "llvm_jit_only_full",
            }
            assert receipts and receipts[0] == expected_receipt, (label, "missing actual pre-call identity")
            expected = bytearray(seeded)
            if case["function"] == 0:
                expected[32:40] = bytes([10, 20, 30, 40, 50, 60, 70, 80])
                expected[22000:27000] = bytes([165]) * 5000
            actual = backing.read_bytes()
            assert actual == expected, (label, "actual shared backing differs; trap may have partially written")
            if case["expected"] == "return":
                assert completed.returncode == 0 and len(receipts) == 2
                assert receipts[1] == dict(expected_receipt, phase="returned")
            else:
                diagnostic = completed.stdout + completed.stderr
                assert (completed.returncode in {-4, -6} or completed.returncode > 0) and len(receipts) == 1
                assert b"memory access out of bounds" in diagnostic, (label, "not a memory bounds trap")
                assert not any(token in diagnostic.lower() for token in
                               [b"materialization failed", b"failed to materialize", b"module emission failed"])
            rows.append({"case": case, "product": product, "component": binary_pin,
                         "returncode": completed.returncode, "receipts": receipts,
                         "before_sha256": digest(seeded), "after_sha256": digest(actual),
                         "full_backing_matches": True,
                         "trap_unchanged_memory": case["expected"] == "trap"})
    result = {"schema": "uwvm.host-provider64-bulk.finite-result.v1", "wasm_sha256": digest(wasm.read_bytes()),
              "rows": rows, "analyzed_module_is_separate_from_actual_runtime_materialization": True,
              "physical_runtime_codegen_qualified": False,
              "qualification_requires": "keeper same-source runtime/component/SDK closure and actual materialized body witness"}
    (args.output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"checked_rows": len(rows), "physical_runtime_codegen_qualified": False}))


if __name__ == "__main__":
    main()

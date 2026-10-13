#!/usr/bin/env python3
"""Pure source/math operation model; never runs a VM, compiler or profiler."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
GENERATOR_SHA = "23f57a8fe61c62e5b613e84e39be702c3d44d2a651a044c9c4ff86275cfa614e"
CHECKER_SHA = "fbc30b4f47979731a0f8e7502dddbd049d5fbd2bff1030ffdf9044ef963b06af"
SOURCE_PATHS = (
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_gc_emit.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_gc_array_get32.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_gc_array_set32.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_emit.h",
    "src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/sealed_local_table_emit.h",
    "src/uwvm2/uwvm/runtime/storage/gc_object.h",
    "src/uwvm2/uwvm/runtime/storage/numeric_array_get32.h",
    "src/uwvm2/uwvm/runtime/storage/numeric_array_set32.h",
    "src/uwvm2/runtime/gc/entry_admission.h",
    "src/uwvm2/runtime/gc/managed_numeric_page.h",
    "src/uwvm2/runtime/gc/sealed_entry_compact_cursor.h",
)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load_checker():
    # Two fixed reviewed sources, not a runtime/source build fingerprint.
    generator = HERE / "generate_general_gc.py"
    checker = HERE / "check_general_gc_generator.py"
    if sha(generator.read_bytes()) != GENERATOR_SHA or sha(checker.read_bytes()) != CHECKER_SHA:
        raise ValueError("general-family generator/oracle bytes changed; review model again")
    spec = importlib.util.spec_from_file_location("general_gc_independent_scalar_model", checker)
    if spec is None or spec.loader is None:
        raise RuntimeError("independent scalar model unavailable")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def source_operation_counts(family, phase, n):
    """Audited source formulas, NOT executed instruction/helper counts.

    G creates allocation groups; O reads old roots before replacing them;
    L loads root groups; M does complete old/final retained readback. Counts
    cover one successful run including mutate setup and final ring traversal.
    Constant-time trap checks, arithmetic and wrapper calls are not counted.
    """
    if type(n) is not int or not 1024 <= n <= 2_000_000:
        raise ValueError("iterations outside reviewed fixture range")
    if family not in ("mutable-struct", "reference-cycle", "numeric-array", "reference-array"):
        raise ValueError("unknown family")
    if phase not in ("allocate", "mutate"):
        raise ValueError("unknown phase")
    roots = 1024
    groups = n if phase == "allocate" else roots
    old = n - roots if phase == "allocate" else 0
    loads = old + roots if phase == "allocate" else n + roots
    readback = old + roots
    counts = {
        "struct.new": 0, "array.new": 0,
        "struct.get.scalar": 0, "struct.get.reference": 0,
        "struct.set.scalar": 0, "struct.set.reference": 0,
        "array.get.scalar": 0, "array.get.reference": 0,
        "array.set.scalar": 0, "array.set.reference": 0,
        "array.len": 0, "table.get": loads, "table.set": groups,
        "ref.cast": loads, "ref.as_non_null": 0, "ref.eq": 0,
    }
    if family == "mutable-struct":
        counts.update({"struct.new": groups, "struct.get.scalar": n + readback,
                       "struct.set.scalar": n})
    elif family == "reference-cycle":
        counts.update({"struct.new": 2 * groups, "struct.get.scalar": 3 * n + 2 * readback,
                       "struct.get.reference": 3 * n + loads + readback,
                       "struct.set.scalar": n, "struct.set.reference": groups + 2 * n,
                       "ref.as_non_null": loads, "ref.eq": n + readback})
    elif family == "numeric-array":
        counts.update({"array.new": groups, "array.get.scalar": 2 * n + 8 * readback,
                       "array.set.scalar": 2 * n, "array.len": n + readback})
    else:
        counts.update({"struct.new": 2 * groups, "array.new": groups,
                       "struct.get.scalar": 2 * n + 8 * readback,
                       "struct.get.reference": n + loads + readback,
                       "struct.set.scalar": n, "struct.set.reference": groups,
                       "array.get.reference": 2 * n + 8 * readback,
                       "array.set.reference": 2 * n, "array.len": n + readback,
                       "table.get": 2 * loads, "table.set": 2 * groups,
                       "ref.cast": 2 * loads, "ref.as_non_null": loads,
                       "ref.eq": n + readback})
    return dict(allocation_groups=groups, old_root_groups_read=old,
                loaded_root_groups=loads, complete_retained_groups_read=readback,
                source_operations=counts,
                source_allocations=counts["struct.new"] + counts["array.new"],
                main_allocations=(counts["struct.new"] + counts["array.new"]) if phase == "allocate" else 0,
                actual_native_helper_calls=None, actual_assembly_instruction_count=None)


def prepare(n):
    model_source = Path(__file__).read_bytes()
    checker = load_checker()
    pinned = {r: (ROOT / r).read_bytes() for r in SOURCE_PATHS}
    rows = []
    for family in checker.g.FAMILIES:
        for phase in checker.g.PHASES:
            binary, _wat, fixture = checker.g.build(family, phase, n)
            independent = checker.scalar_reference(family, phase, n)
            expected = fixture["expected"]
            keys = ("step_checksum_u32", "root_checksum_u32", "return_checksum_u32", "last_lcg_u32")
            if tuple(expected[key] for key in keys) != independent:
                raise AssertionError("independent scalar checksum differs")
            model = source_operation_counts(family, phase, n)
            if model["source_allocations"] != expected["guest_planned_allocations"] or model["main_allocations"] != expected["guest_main_allocations"]:
                raise AssertionError("allocation formula and separate fixture contract differ")
            rows.append(dict(family=family, phase=phase, iterations=n, wasm_sha256=sha(binary),
                             independent_scalar_checksums=dict(zip(keys, independent)),
                             table_root_slots=fixture["table_root_slots"], **model))
    if any((ROOT / r).read_bytes() != data for r, data in pinned.items()):
        raise RuntimeError("source changed during read-only model preparation")
    load_checker()  # Explicit byte closure for the generator/independent oracle.
    if Path(__file__).read_bytes() != model_source:
        raise RuntimeError("operation model source changed during preparation")
    return dict(schema="uwvm-general-gc-source-hot-operation-model-v1", source_only=True,
                model_script_sha256=sha(model_source),
                generator_sha256=GENERATOR_SHA, independent_oracle_sha256=CHECKER_SHA,
                source_pins={r: dict(bytes=len(data), sha256=sha(data)) for r, data in pinned.items()},
                rows=rows, native_execution=False, wasm_vm_execution=False,
                official_wasm_validation=False, performance_qualified=False,
                counts_scope="successful source-level run including setup/final readback; not measured native calls or emitted machine instructions")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iterations", type=int, default=65536)
    parser.add_argument("--out", type=Path, help="new JSON file only; prior evidence is never overwritten")
    args = parser.parse_args()
    raw = (json.dumps(prepare(args.iterations), indent=2) + "\n").encode()
    if args.out is None:
        sys.stdout.write(raw.decode())
    else:
        # Reuse only the generator's small atomic new-file publisher. No VM
        # loader, profiler wrapper, process/cgroup guard or native API is run.
        checker = load_checker()
        checker.g.write_new(args.out, raw)
        print(json.dumps(dict(path=str(args.out), bytes=len(raw), sha256=sha(raw), source_only=True)))


if __name__ == "__main__":
    main()

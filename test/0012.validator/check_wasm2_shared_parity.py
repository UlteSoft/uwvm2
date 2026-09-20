#!/usr/bin/env python3
"""Check deliberately shared UWVM2/ROS implementations for synchronization drift.

This is a source-parity guard, not a Wasm conformance or semantic-equivalence
proof. ROS-specific validators, initializers and execution policies still need
their execution tests. Do not make those implementations byte-identical.
"""
import argparse
import hashlib
import json
import pathlib

COMMON_FILES = (
    "runtime/compiler/shared/strict_float.h",
    "runtime/compiler/shared/wasm1p1_simd.h",
    "runtime/compiler/shared/wasm1p1_simd.cppm",
    "runtime/compiler/uwvm_int/optable/numeric.h",
    "runtime/compiler/uwvm_int/optable/convert.h",
    "runtime/compiler/uwvm_int/optable/memory.h",
    "runtime/compiler/uwvm_int/optable/memory.cppm",
    "runtime/compiler/uwvm_int/optable/conbine.h",
    "runtime/compiler/uwvm_int/optable/conbine_heavy.h",
    "runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/memory_emit.h",
    "runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/simd_emit.h",
    "runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/host_address_emit.h",
    "runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h",
    "runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.cppm",
    "runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/macho_headers.h",
    "runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/dwarf_eh_frame_registration.h",
    "runtime/lib/uwvm_runtime_checked_size.h",
    "runtime/lib/uwvm_runtime_native_stack_guard.h",
    # Module type ownership and native-Windows WSA lifetime are shared even
    # though the products intentionally expose different execution policies.
    "runtime/lib/uwvm_runtime.cppm",
    "uwvm/crtmain/uwvm.h",
)
COMMON_TREES = (
    "object/global",
    "object/memory/linear",
    "runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/opcode",
)
FULL_ONLY = (
    "runtime/compiler/uwvm_int/optable/lazy.h",
    "runtime/compiler/uwvm_int/optable/lazy.cppm",
    "runtime/compiler/uwvm_int/compile_cu_from_lazy_validator",
    "runtime/compiler/llvm_jit/compile_cu_from_lazy_validator",
)
CONTROL = "runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/opcode/control_flow_cases.h"
TIERED_OUTPUT = "            else if(tiered_loop_reentries_out != nullptr) { *tiered_loop_reentries_out = llvm_jit_emit_state.tiered_loop_reentries; }\n"
MEMORY = "runtime/compiler/uwvm_int/optable/memory.h"
MEMORY_COMMENTS = (
    '        // Mirror the JIT\'s "definitely fail" front-end for native memories: if the current page count already exceeds\n',
    '        // Mirror the LLVM AOT backend\'s "definitely fail" front-end for native memories: if the current page count already exceeds\n',
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--main", type=pathlib.Path, required=True)
    parser.add_argument("--ros", type=pathlib.Path, required=True)
    args = parser.parse_args()
    roots = [path.resolve() / "src/uwvm2" for path in (args.main, args.ros)]
    if roots[0] == roots[1] or not all(path.is_dir() for path in roots):
        parser.error("two distinct, existing product roots are required")
    files = set(COMMON_FILES)
    errors = []
    records = []
    for tree in COMMON_TREES:
        inventories = []
        for root in roots:
            inventories.append({str(path.relative_to(root)) for path in (root / tree).rglob("*")
                                if path.is_file() and path.suffix in (".h", ".cppm")})
        if not inventories[0] or inventories[0] != inventories[1]:
            errors.append({"tree": tree, "main_only": sorted(inventories[0] - inventories[1]),
                           "ros_only": sorted(inventories[1] - inventories[0])})
        files.update(inventories[0] | inventories[1])
    for relative in sorted(files):
        paths = [root / relative for root in roots]
        if not all(path.is_file() for path in paths):
            errors.append({"file": relative, "error": "missing"})
            continue
        data = [path.read_bytes() for path in paths]
        digest = [hashlib.sha256(item).hexdigest() for item in data]
        if relative == CONTROL:
            omission = TIERED_OUTPUT.encode()
            if data[0].count(omission) != 1 or omission in data[1]:
                errors.append({"file": relative, "error": "unexpected tiered-output shape"})
            data[0] = data[0].replace(omission, b"")
        if relative == MEMORY:
            # This exact terminology-only comment is the sole permitted
            # difference; retain byte comparison of every executable line.
            comments = [comment.encode() for comment in MEMORY_COMMENTS]
            if any(item.count(comment) != 1 for item, comment in zip(data, comments)):
                errors.append({"file": relative, "error": "unexpected memory-growth comment shape"})
            data = [item.replace(comment, b"") for item, comment in zip(data, comments)]
        if data[0] != data[1]:
            errors.append({"file": relative, "error": "shared implementation drift"})
        records.append({"file": relative, "main_sha256": digest[0], "ros_sha256": digest[1]})
    for relative in FULL_ONLY:
        if not (roots[0] / relative).exists() or (roots[1] / relative).exists():
            errors.append({"path": relative, "error": "full-only mode boundary changed"})
    print(json.dumps({"shared_files_checked": len(records),
                      "intentional_control_flow_difference": "ROS omits tiered reentry output",
                      "intentional_memory_comment_difference": "JIT versus LLVM AOT backend terminology only",
                      "removed_mode_paths_checked": len(FULL_ONLY),
                      "failures": errors, "files": records}, indent=2))
    return bool(errors)


if __name__ == "__main__":
    raise SystemExit(main())

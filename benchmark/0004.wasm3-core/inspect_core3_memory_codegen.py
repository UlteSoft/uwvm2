#!/usr/bin/env python3
"""Capture actual O3 product JIT objects for selected Core 3 guest loops.

The selected loops use the same WAT generator as the paired benchmark. Sparse
high-address memory64 additionally requires the bounded semantic preflight.
This is an assembly/semantic check, not timing.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

from generate import module, page_memory_trace
from run import (LIB_PATHS, cgroup_preflight, command, sha256,
                 require_sparse_high_memory_headroom, verify_product_build)


DEFAULT_CASES = ("memory32-page-aligned-store", "memory32-page-unaligned-store",
                 "memory64-page-aligned-store", "memory64-page-unaligned-store")
SELECTABLE_CASES = (*DEFAULT_CASES, "memory32-random-store", "memory64-random-store",
                    "memory64-high-random-store", "multi-memory-random-store",
                    "memory32-atomic-rmw", "memory64-atomic-rmw",
                    "gc-struct-heap-update", "gc-array-heap-update",
                    "gc-allocation-ring", "gc-cast",
                    "table32-indirect", "table64-indirect", "call-ref-step",
                    "direct-call-step", "strict-swizzle", "relaxed-swizzle")
TWO_TARGET_CASES = {"table32-indirect", "table64-indirect",
                    "call-ref-step", "direct-call-step"}


def guest_object(source, cache, ros):
    objects = list(cache.rglob("*.uwvm-ljc"))
    if len(objects) != 1:
        raise RuntimeError(f"expected one actual product JIT cache object, got {len(objects)}")
    sys.path.insert(0, str(source / "test/0014.llvm_jit"))
    from check_wasm3_native_frame_codegen import decode_object
    return objects[0], decode_object(objects[0].read_bytes(), ros)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--product", required=True, type=Path)
    parser.add_argument("--expected-source-id", required=True)
    parser.add_argument("--wasm-tools", required=True, type=Path)
    parser.add_argument("--objdump", type=Path, default=Path("/toolchain/bin/llvm-objdump"))
    parser.add_argument("--nm", type=Path, default=Path("/toolchain/bin/llvm-nm"))
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--case", action="append", choices=SELECTABLE_CASES,
                        help="capture selected Core 3 guest loop; defaults to four page-store controls")
    parser.add_argument("--high-memory-preflight", type=Path,
                        help="required approved bounded preflight for sparse >4 GiB memory64 capture")
    parser.add_argument("--count", type=int, default=8192)
    parser.add_argument("--cpu", type=int, default=0)
    args = parser.parse_args()
    if args.out.exists() or not 8192 <= args.count <= 100000:
        parser.error("fresh output and 8192..100000 iterations required")
    selected_cases = args.case or list(DEFAULT_CASES)
    if len(set(selected_cases)) != len(selected_cases):
        parser.error("select each --case only once")
    high_case = "memory64-high-random-store" in selected_cases
    if high_case and (args.high_memory_preflight is None or args.count > 10000):
        parser.error("sparse >4 GiB capture requires matching preflight and count <=10000")
    state_before = cgroup_preflight(args.cpu)
    if high_case:
        require_sparse_high_memory_headroom(state_before)
    args.out.mkdir(parents=True)
    shutil.copyfile(__file__, args.out / Path(__file__).name)
    shutil.copyfile(Path(__file__).with_name("generate.py"), args.out / "generate.py")
    shutil.copyfile(Path(__file__).with_name("run.py"), args.out / "run.py")
    build = verify_product_build("product", args.product, args.out, "before")
    if build["source_id"] != args.expected_source_id:
        raise RuntimeError("JIT object capture product differs from intended source")
    source = Path(build["source"])
    engine = "ros-jit" if args.ros else "ordinary-jit"
    high_preflight = None
    if high_case:
        summary_path = args.high_memory_preflight.resolve(strict=True)
        high_preflight = json.loads(summary_path.read_text())
        preflight_meta_path = summary_path.parent / "metadata.json"
        preflight_meta = json.loads(preflight_meta_path.read_text())
        first_events = dict(line.split() for line in
                            preflight_meta["cgroup_before"]["memory.events"].splitlines())
        last_events = dict(line.split() for line in
                           preflight_meta["cgroup_after"]["memory.events"].splitlines())
        if (engine not in high_preflight["approved_engines"] or
                preflight_meta["source_builds"]["ros" if args.ros else "ordinary"] != build or
                preflight_meta["engine_sha256"].get(engine) != sha256(args.product) or
                preflight_meta["wasm_tools_sha256"] != sha256(args.wasm_tools) or
                preflight_meta["generator_sha256"] != sha256(Path(__file__).with_name("generate.py")) or
                preflight_meta["iterations"] < args.count or
                preflight_meta["guest_footprint"]["final_expected"] == 0 or
                any(first_events[name] != last_events[name] for name in ("oom", "oom_kill"))):
            raise RuntimeError("sparse high-address JIT capture lacks matching safe preflight")
        high_preflight = {"path": str(summary_path), "sha256": sha256(summary_path),
                          "metadata_path": str(preflight_meta_path),
                          "metadata_sha256": sha256(preflight_meta_path),
                          "approved_engines": high_preflight["approved_engines"]}
    tool_paths = {"wasm-tools": args.wasm_tools, "objdump": args.objdump,
                  "nm": args.nm,
                  "decoder": source / "test/0014.llvm_jit/check_wasm3_native_frame_codegen.py"}
    if high_case:
        watchdog_source = Path(__file__).with_name("preflight_high_memory.py")
        shutil.copyfile(watchdog_source, args.out / watchdog_source.name)
        tool_paths["sparse_watchdog"] = watchdog_source
    tool_paths.update({name: args.out / name for name in
                       ("generate.py", "run.py", Path(__file__).name)})
    tool_hashes = {name: sha256(path) for name, path in tool_paths.items()}
    env = os.environ.copy()
    env["LD_LIBRARY_PATH"] = ":".join((*LIB_PATHS, env.get("LD_LIBRARY_PATH", "")))
    rows = []
    if high_case:
        from preflight_high_memory import MAX_RSS_KIB, bounded_run
        bounded_logs = args.out / "bounded-logs"
        bounded_logs.mkdir()
    for case in selected_cases:
        wat_source, features = module(case, args.count)
        wat = args.out / f"{case}-{args.count}.wat"
        wasm = wat.with_suffix(".wasm")
        wat.write_text(wat_source)
        subprocess.run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)], check=True)
        subprocess.run([str(args.wasm_tools), "validate", str(wasm)], check=True)
        boundary = None
        if case in DEFAULT_CASES:
            boundary = page_memory_trace(args.count, "-aligned-" in case)
            if "-unaligned-" in case and boundary["crossing_stores"] <= 0:
                raise RuntimeError(f"{case}: unaligned workload did not actually cross a page")
            if "-aligned-" in case and boundary["crossing_stores"] != 0:
                raise RuntimeError(f"{case}: aligned control crossed a page")
        for policy in ("instruction", "unwind"):
            cache = args.out / f"{case}-{policy}-cache"
            cache.mkdir(mode=0o700)
            invocation = command(engine, args.product, wasm, features, policy, args.cpu)
            offset = invocation.index("-Rllvm-cache-path")
            invocation[offset + 1:offset + 2] = ["path", str(cache)]
            watchdog = None
            if case == "memory64-high-random-store":
                watchdog = bounded_run(invocation, f"{case}-{policy}", bounded_logs, env)
                outcome_exit = watchdog["exit"]
                outcome_log = Path(watchdog["log"]).read_bytes()
                if watchdog["maxrss_kib"] > MAX_RSS_KIB:
                    raise RuntimeError(f"{case}/{policy}: exceeded approved 2 GiB RSS")
            else:
                outcome = subprocess.run(invocation, env=env, capture_output=True, timeout=60)
                outcome_exit = outcome.returncode
                outcome_log = outcome.stdout + outcome.stderr
            log = args.out / f"{case}-{policy}-run.log"
            log.write_bytes(outcome_log)
            if outcome_exit:
                raise RuntimeError(f"{case}/{policy}: product self-check failed; see {log}")
            artifact, object_bytes = guest_object(source, cache, args.ros)
            guest = args.out / f"{case}-{policy}-guest.o"
            guest.write_bytes(object_bytes)
            symbols = subprocess.check_output(
                [str(args.nm), "--defined-only", str(guest)], env=env, text=True)
            (args.out / f"{case}-{policy}-symbols.txt").write_text(symbols)
            function_index = 2 if case in TWO_TARGET_CASES else 0
            matches = re.findall(r"\b(uwvm_m_[0-9a-f]+_func_" +
                                 str(function_index) + r")$", symbols, re.M)
            if len(matches) != 1:
                raise RuntimeError(f"{case}/{policy}: expected one guest _func_{function_index}, "
                                   f"got {matches}")
            disassembly = subprocess.check_output(
                [str(args.objdump), "-dr", "--no-show-raw-insn",
                 "--disassemble-symbols=" + matches[0], str(guest)], env=env, text=True)
            listing = args.out / f"{case}-{policy}-assembly.txt"
            listing.write_text(disassembly)
            if "file format elf64-x86-64" not in disassembly:
                raise RuntimeError(f"{case}/{policy}: expected exact Linux x86-64 JIT object")
            instructions = [matched.group(1) for line in disassembly.splitlines()
                            if (matched := re.match(r"^\s*[0-9a-f]+:\s+([a-z][a-z0-9.]*)\b", line))]
            if not instructions or not any(op.startswith("ret") for op in instructions):
                raise RuntimeError(f"{case}/{policy}: disassembly missed function body/return")
            rows.append({"case": case, "trace": policy, "command": invocation,
                         "sparse_memory_watchdog": watchdog,
                         "guest_object_sha256": sha256(guest),
                         "cache_sha256": sha256(artifact),
                         "assembly_sha256": sha256(listing),
                         "run_log_sha256": sha256(log),
                         "wat_sha256": sha256(wat), "wasm_sha256": sha256(wasm),
                         "boundary_trace": boundary,
                         "guest_symbol": matches[0],
                         "guest_function_instruction_count": len(instructions),
                         "guest_function_conditional_jumps": sum(
                             op.startswith("j") and op not in ("jmp", "jmpq")
                             for op in instructions),
                         "guest_function_calls": sum(op.startswith("call") for op in instructions),
                         "guest_function_lock_prefixes": sum(op == "lock" for op in instructions)})
            (args.out / "rows.json").write_text(json.dumps(rows, indent=2) + "\n")
    after = verify_product_build("product", args.product, args.out, "after")
    state_after = cgroup_preflight(args.cpu)
    first_events = dict(line.split() for line in state_before["memory.events"].splitlines())
    last_events = dict(line.split() for line in state_after["memory.events"].splitlines())
    if (after != build or sha256(args.product) != build["binary_sha256"] or
            (high_preflight is not None and
             (sha256(Path(high_preflight["path"])) != high_preflight["sha256"] or
              sha256(Path(high_preflight["metadata_path"])) !=
                  high_preflight["metadata_sha256"])) or
            any(sha256(path) != tool_hashes[name] for name, path in tool_paths.items()) or
            any(sha256(Path(__file__).with_name(name)) != tool_hashes[name]
                for name in ("generate.py", "run.py", Path(__file__).name)) or
            any(first_events[name] != last_events[name] for name in ("oom", "oom_kill"))):
        raise RuntimeError("source, tool, binary or cgroup OOM changed during capture")
    result = {"scope": "actual JIT object assembly; not a wall-clock benchmark",
              "selected_cases": selected_cases, "high_memory_preflight": high_preflight,
              "source_build": build, "source_build_after": after,
              "expected_source_id": args.expected_source_id,
              "product_binary_sha256": sha256(args.product),
              "tools_sha256": tool_hashes,
              "cgroup_before": state_before, "cgroup_after": state_after,
              "end_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
              "rows": rows}
    (args.out / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    print("PASS actual O3 JIT Core 3 selected guest-loop object capture")


if __name__ == "__main__":
    main()

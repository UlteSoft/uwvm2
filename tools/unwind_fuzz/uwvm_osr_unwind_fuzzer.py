#!/usr/bin/env python3
import argparse
import os
import random
import re
import shutil
import subprocess
from pathlib import Path


ANSI_RE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
FUNC_RE = re.compile(r"func_idx=(\d+)")
TRAP_RE = re.compile(r"Runtime crash \(([^)]+)\)")


TRAPS = (
    "unreachable",
    "i32_div_s_zero",
    "i32_div_u_zero",
    "i32_overflow",
    "i64_div_s_zero",
    "i64_div_u_zero",
    "i64_overflow",
    "invalid_i32_trunc_f32_s",
    "invalid_i32_trunc_f32_u",
    "invalid_i32_trunc_f64_s",
    "invalid_i32_trunc_f64_u",
    "invalid_i64_trunc_f32_s",
    "invalid_i64_trunc_f32_u",
    "invalid_i64_trunc_f64_s",
    "invalid_i64_trunc_f64_u",
    "oob_load",
    "oob_store",
    "call_indirect_type",
    "call_indirect_null",
    "call_indirect_oob",
)


POLICIES = ("instruction", "unwind", "auto")


OSR_MODES = (
    ("tiered", ["-Rtiered"]),
    ("tiered_full_ready", ["-Rtiered", "-Rct", "2", "-Rllvm-policy", "max"]),
    ("tiered_no_t2", ["-Rtiered", "-Rtiered-disable-t2"]),
)


def strip_ansi(s: str) -> str:
    return ANSI_RE.sub("", s)


def parse_output(s: str):
    plain = strip_ansi(s)
    trap_match = TRAP_RE.search(plain)
    return (trap_match.group(1) if trap_match else "", [int(m.group(1)) for m in FUNC_RE.finditer(plain)])


def leaf_prefix_and_body(trap: str):
    if trap == "unreachable":
        return "", "    unreachable"
    if trap == "i32_div_s_zero":
        return "", "    i32.const 123\n    i32.const 0\n    i32.div_s\n    drop"
    if trap == "i32_div_u_zero":
        return "", "    i32.const 123\n    i32.const 0\n    i32.div_u\n    drop"
    if trap == "i32_overflow":
        return "", "    i32.const -2147483648\n    i32.const -1\n    i32.div_s\n    drop"
    if trap == "i64_div_s_zero":
        return "", "    i64.const 123\n    i64.const 0\n    i64.div_s\n    drop"
    if trap == "i64_div_u_zero":
        return "", "    i64.const 123\n    i64.const 0\n    i64.div_u\n    drop"
    if trap == "i64_overflow":
        return "", "    i64.const -9223372036854775808\n    i64.const -1\n    i64.div_s\n    drop"
    if trap == "invalid_i32_trunc_f32_s":
        return "", "    f32.const nan\n    i32.trunc_f32_s\n    drop"
    if trap == "invalid_i32_trunc_f32_u":
        return "", "    f32.const nan\n    i32.trunc_f32_u\n    drop"
    if trap == "invalid_i32_trunc_f64_s":
        return "", "    f64.const nan\n    i32.trunc_f64_s\n    drop"
    if trap == "invalid_i32_trunc_f64_u":
        return "", "    f64.const nan\n    i32.trunc_f64_u\n    drop"
    if trap == "invalid_i64_trunc_f32_s":
        return "", "    f32.const nan\n    i64.trunc_f32_s\n    drop"
    if trap == "invalid_i64_trunc_f32_u":
        return "", "    f32.const nan\n    i64.trunc_f32_u\n    drop"
    if trap == "invalid_i64_trunc_f64_s":
        return "", "    f64.const nan\n    i64.trunc_f64_s\n    drop"
    if trap == "invalid_i64_trunc_f64_u":
        return "", "    f64.const nan\n    i64.trunc_f64_u\n    drop"
    if trap == "oob_load":
        return "  (memory 1)\n", "    i32.const 2147483647\n    i32.load\n    drop"
    if trap == "oob_store":
        return "  (memory 1)\n", "    i32.const 2147483647\n    i64.const 7\n    i64.store"
    if trap == "call_indirect_type":
        return (
            "  (type $i (func (param i32)))\n"
            "  (table 1 funcref)\n"
            "  (elem (i32.const 0) $target)\n"
            "  (func $target (type $i) (param $x i32)\n"
            "    local.get $x\n"
            "    drop)\n",
            "    i32.const 0\n    call_indirect (type $v)",
        )
    if trap == "call_indirect_null":
        return "  (table 1 funcref)\n", "    i32.const 0\n    call_indirect (type $v)"
    if trap == "call_indirect_oob":
        return "  (table 1 funcref)\n", "    i32.const 7\n    call_indirect (type $v)"
    raise AssertionError(trap)


def caller_chain(depth: int) -> str:
    previous = "$loop_then_trap"
    lines = []
    for i in range(depth):
        name = f"$caller{i}"
        lines.append(f"  (func {name} (type $v) call {previous})")
        previous = name
    lines.append(f'  (func $_start (export "_start") (type $v) call {previous})')
    return "\n".join(lines)


def make_wat(trap: str, depth: int, nops: int, loop_count: int) -> str:
    prefix, leaf = leaf_prefix_and_body(trap)
    nop_text = "".join("    nop\n" for _ in range(nops))
    return (
        "(module\n"
        "  (type $v (func))\n"
        f"{prefix}"
        "  (func $leaf (type $v)\n"
        f"{leaf})\n"
        "  (func $loop_then_trap (type $v)\n"
        "    (local $i i32)\n"
        f"{nop_text}"
        "    i32.const 0\n"
        "    local.set $i\n"
        "    block\n"
        "      loop\n"
        "        local.get $i\n"
        f"        i32.const {loop_count}\n"
        "        i32.ge_u\n"
        "        br_if 1\n"
        "        local.get $i\n"
        "        i32.const 1\n"
        "        i32.add\n"
        "        local.set $i\n"
        "        br 0\n"
        "      end\n"
        "    end\n"
        "    call $leaf)\n"
        f"{caller_chain(depth)}\n"
        ")\n"
    )


def run(args, cwd: Path):
    return subprocess.run(args, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True, type=Path)
    ap.add_argument("--uwvm", required=True, type=Path)
    ap.add_argument("--wat2wasm", default=shutil.which("wat2wasm"))
    ap.add_argument("--cases", type=int, default=256)
    ap.add_argument("--seed", type=int, default=0x05A0F00D)
    ap.add_argument("--work-dir", type=Path, default=None)
    ns = ap.parse_args()
    rng = random.Random(ns.seed)
    base = ns.work_dir if ns.work_dir is not None else Path("/tmp")
    work = base / f"uwvm_osr_unwind_fuzz_{os.getpid()}"
    work.mkdir(parents=True, exist_ok=True)
    print(f"[osr-fuzz] work={work}")
    print(f"[osr-fuzz] seed={ns.seed} cases={ns.cases}")

    failures = []
    skipped_no_osr = 0
    total_runs = 0
    for case_id in range(ns.cases):
        trap = rng.choice(TRAPS)
        depth = rng.randint(0, 4)
        nops = rng.randint(1500, 1800)
        loop_count = rng.randint(300000, 500000)
        wat = make_wat(trap, depth, nops, loop_count)
        wat_path = work / f"osr_{case_id:04d}_{trap}_d{depth}.wat"
        wasm_path = work / f"osr_{case_id:04d}_{trap}_d{depth}.wasm"
        wat_path.write_text(wat)
        r = run([str(ns.wat2wasm), str(wat_path), "-o", str(wasm_path)], ns.root)
        if r.returncode != 0:
            failures.append((case_id, trap, "wat2wasm", r.stdout, wat_path))
            continue

        case_failure_count = len(failures)
        skipped_modes = []
        for mode_name, mode_args in OSR_MODES:
            results = []
            for policy in POLICIES:
                out_path = work / f"osr_{case_id:04d}_{trap}_d{depth}.{mode_name}.{policy}.out"
                log_path = work / f"osr_{case_id:04d}_{trap}_d{depth}.{mode_name}.{policy}.log"
                args = [
                    str(ns.uwvm),
                    *mode_args,
                    "-Rllvm-call-stack",
                    policy,
                    "-Rclog",
                    "file",
                    str(log_path),
                    "--run",
                    str(wasm_path),
                ]
                r = run(args, ns.root)
                total_runs += 1
                log = log_path.read_text() if log_path.exists() else ""
                log_path.unlink(missing_ok=True)
                parsed = parse_output(r.stdout)
                if r.returncode == 0 or not parsed[1]:
                    out_path.write_text(r.stdout)
                    failures.append((case_id, trap, f"{mode_name}/{policy}/no-trap-or-empty-stack", r.stdout, wat_path))
                    continue
                results.append((policy, parsed, "tiered-osr-request" in log, out_path, r.stdout, log))

            if len(results) == len(POLICIES):
                osr_count = sum(1 for _, _, has_osr, _, _, _ in results if has_osr)
                if osr_count == 0:
                    skipped_no_osr += 1
                    skipped_modes.append(mode_name)
                    continue
                if osr_count != len(results):
                    for policy, _, has_osr, _, stdout, log in results:
                        if not has_osr:
                            failures.append((case_id, trap, f"{mode_name}/{policy}/missing-osr-request", log + "\n" + stdout, wat_path))
                    continue

                baseline = None
                baseline_out = None
                baseline_stdout = None
                for policy, parsed, _, out_path, stdout, _ in results:
                    if policy == "instruction":
                        baseline = parsed
                        baseline_out = out_path
                        baseline_stdout = stdout
                    elif baseline is not None and parsed != baseline:
                        if baseline_stdout is not None and baseline_out is not None:
                            baseline_out.write_text(baseline_stdout)
                        out_path.write_text(stdout)
                        failures.append(
                            (
                                case_id,
                                trap,
                                f"{mode_name}/{policy}/mismatch baseline={baseline} actual={parsed} baseline_out={baseline_out} out={out_path}",
                                stdout,
                                wat_path,
                            )
                        )

        if len(failures) == case_failure_count:
            wat_path.unlink(missing_ok=True)
            wasm_path.unlink(missing_ok=True)

        skipped_suffix = f" skipped=no-osr:{','.join(skipped_modes)}" if skipped_modes else ""
        print(f"[osr-fuzz] completed {case_id + 1}/{ns.cases}{skipped_suffix}")

    if failures:
        print(f"[osr-fuzz] FAIL failures={len(failures)} total_runs={total_runs} skipped_no_osr={skipped_no_osr}")
        for case_id, trap, where, output, wat_path in failures[:8]:
            print(f"[osr-fuzz] failure case={case_id} trap={trap} where={where} wat={wat_path}")
            print(output[:1200])
        return 1

    shutil.rmtree(work, ignore_errors=True)
    print(f"[osr-fuzz] PASS cases={ns.cases} total_runs={total_runs} skipped_no_osr={skipped_no_osr}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

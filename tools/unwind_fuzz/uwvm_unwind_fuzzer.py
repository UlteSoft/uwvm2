#!/usr/bin/env python3
import argparse
import os
import random
import re
import shutil
import subprocess
import sys
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


MODES = (
    ("tiered", ["-Rtiered"]),
    ("tiered_full_ready", ["-Rtiered", "-Rct", "2", "-Rllvm-policy", "max"]),
    ("tiered_no_t0", ["-Rtiered", "-Rtiered-disable-t0"]),
    ("tiered_no_t2", ["-Rtiered", "-Rtiered-disable-t2"]),
    ("tiered_no_t0_no_t2", ["-Rtiered", "-Rtiered-disable-t0", "-Rtiered-disable-t2"]),
)


POLICIES = ("instruction", "unwind", "auto")


def strip_ansi(s: str) -> str:
    return ANSI_RE.sub("", s)


def parse_output(s: str):
    plain = strip_ansi(s)
    trap_match = TRAP_RE.search(plain)
    trap = trap_match.group(1) if trap_match else ""
    funcs = [int(m.group(1)) for m in FUNC_RE.finditer(plain)]
    return trap, funcs


def random_padding(rng: random.Random, min_n: int = 0, max_n: int = 16) -> str:
    return "".join("    nop\n" for _ in range(rng.randint(min_n, max_n)))


def chain_functions(depth: int, rng: random.Random, leaf_name: str = "$leaf") -> str:
    lines = []
    previous = leaf_name
    for i in range(depth):
        name = f"$f{i}"
        style = rng.choice(("plain", "padded", "block", "if"))
        if style == "plain":
            lines.append(f"  (func {name} (type $v) call {previous})")
        elif style == "padded":
            lines.append(
                f"  (func {name} (type $v)\n"
                f"{random_padding(rng, 1, 8)}"
                f"    i32.const {rng.randint(1, 100)}\n"
                f"    i32.const {rng.randint(1, 100)}\n"
                "    i32.add\n"
                "    drop\n"
                f"    call {previous})"
            )
        elif style == "block":
            lines.append(
                f"  (func {name} (type $v)\n"
                "    block\n"
                f"{random_padding(rng, 0, 6)}"
                f"      call {previous}\n"
                "    end)"
            )
        else:
            lines.append(
                f"  (func {name} (type $v) (local $x i32)\n"
                f"    i32.const {rng.choice([0, 1])}\n"
                "    local.set $x\n"
                "    local.get $x\n"
                "    if\n"
                f"      call {previous}\n"
                "    else\n"
                f"      call {previous}\n"
                "    end)"
            )
        previous = name
    lines.append(f'  (func $_start (export "_start") (type $v) call {previous})')
    return "\n".join(lines)


def leaf_body(trap: str, rng: random.Random) -> tuple[str, str]:
    pad = random_padding(rng)
    if trap == "unreachable":
        return "", f"{pad}    unreachable"
    if trap == "i32_div_s_zero":
        return "", f"{pad}    i32.const {rng.randint(-1000, 1000)}\n    i32.const 0\n    i32.div_s\n    drop"
    if trap == "i32_div_u_zero":
        return "", f"{pad}    i32.const {rng.randint(0, 1000)}\n    i32.const 0\n    i32.div_u\n    drop"
    if trap == "i32_overflow":
        return "", f"{pad}    i32.const -2147483648\n    i32.const -1\n    i32.div_s\n    drop"
    if trap == "i64_div_s_zero":
        return "", f"{pad}    i64.const {rng.randint(-1000, 1000)}\n    i64.const 0\n    i64.div_s\n    drop"
    if trap == "i64_div_u_zero":
        return "", f"{pad}    i64.const {rng.randint(0, 1000)}\n    i64.const 0\n    i64.div_u\n    drop"
    if trap == "i64_overflow":
        return "", f"{pad}    i64.const -9223372036854775808\n    i64.const -1\n    i64.div_s\n    drop"
    if trap == "invalid_i32_trunc_f32_s":
        return "", f"{pad}    f32.const nan\n    i32.trunc_f32_s\n    drop"
    if trap == "invalid_i32_trunc_f32_u":
        return "", f"{pad}    f32.const nan\n    i32.trunc_f32_u\n    drop"
    if trap == "invalid_i32_trunc_f64_s":
        return "", f"{pad}    f64.const nan\n    i32.trunc_f64_s\n    drop"
    if trap == "invalid_i32_trunc_f64_u":
        return "", f"{pad}    f64.const nan\n    i32.trunc_f64_u\n    drop"
    if trap == "invalid_i64_trunc_f32_s":
        return "", f"{pad}    f32.const nan\n    i64.trunc_f32_s\n    drop"
    if trap == "invalid_i64_trunc_f32_u":
        return "", f"{pad}    f32.const nan\n    i64.trunc_f32_u\n    drop"
    if trap == "invalid_i64_trunc_f64_s":
        return "", f"{pad}    f64.const nan\n    i64.trunc_f64_s\n    drop"
    if trap == "invalid_i64_trunc_f64_u":
        return "", f"{pad}    f64.const nan\n    i64.trunc_f64_u\n    drop"
    if trap == "oob_load":
        return "  (memory 1)\n", f"{pad}    i32.const {rng.choice([65536, 131072, 2147483647])}\n    i32.load\n    drop"
    if trap == "oob_store":
        return "  (memory 1)\n", f"{pad}    i32.const {rng.choice([65536, 131072, 2147483647])}\n    i64.const {rng.randint(1, 100)}\n    i64.store"
    if trap == "call_indirect_type":
        prefix = (
            "  (type $i (func (param i32)))\n"
            "  (table 1 funcref)\n"
            "  (elem (i32.const 0) $target)\n"
            "  (func $target (type $i) (param $x i32)\n"
            "    local.get $x\n"
            "    drop)\n"
        )
        return prefix, f"{pad}    i32.const 0\n    call_indirect (type $v)"
    if trap == "call_indirect_null":
        return "  (table 1 funcref)\n", f"{pad}    i32.const 0\n    call_indirect (type $v)"
    if trap == "call_indirect_oob":
        return "  (table 1 funcref)\n", f"{pad}    i32.const {rng.choice([1, 2, 100])}\n    call_indirect (type $v)"
    raise AssertionError(trap)


def make_wat(trap: str, depth: int, rng: random.Random) -> str:
    prefix, body = leaf_body(trap, rng)
    return (
        "(module\n"
        "  (type $v (func))\n"
        f"{prefix}"
        "  (func $leaf (type $v)\n"
        f"{body})\n"
        f"{chain_functions(depth, rng)}\n"
        ")\n"
    )


def run_cmd(args, cwd: Path):
    return subprocess.run(args, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True, type=Path)
    ap.add_argument("--uwvm", required=True, type=Path)
    ap.add_argument("--wat2wasm", default=shutil.which("wat2wasm"))
    ap.add_argument("--cases", type=int, default=512)
    ap.add_argument("--seed", type=int, default=0xC0DEF00D)
    ap.add_argument("--work-dir", type=Path, default=None)
    ns = ap.parse_args()

    if not ns.wat2wasm:
        print("wat2wasm not found", file=sys.stderr)
        return 2

    rng = random.Random(ns.seed)
    base = ns.work_dir if ns.work_dir is not None else Path("/tmp")
    work = base / f"uwvm_unwind_fuzz_{os.getpid()}"
    work.mkdir(parents=True, exist_ok=True)
    print(f"[fuzz] work={work}")
    print(f"[fuzz] seed={ns.seed} cases={ns.cases}")

    failures = []
    total_runs = 0
    for case_id in range(ns.cases):
        trap = rng.choice(TRAPS)
        depth = rng.randint(1, 12)
        wat = make_wat(trap, depth, rng)
        wat_path = work / f"case_{case_id:04d}_{trap}_d{depth}.wat"
        wasm_path = work / f"case_{case_id:04d}_{trap}_d{depth}.wasm"
        wat_path.write_text(wat)

        r = run_cmd([str(ns.wat2wasm), str(wat_path), "-o", str(wasm_path)], ns.root)
        if r.returncode != 0:
            failures.append((case_id, trap, "wat2wasm", r.stdout, wat_path))
            continue

        case_failure_count = len(failures)
        for mode_name, mode_args in MODES:
            baseline = None
            baseline_out = None
            baseline_stdout = None
            for policy in POLICIES:
                args = [str(ns.uwvm), *mode_args, "-Rllvm-call-stack", policy, "--run", str(wasm_path)]
                r = run_cmd(args, ns.root)
                total_runs += 1
                out_path = work / f"case_{case_id:04d}_{trap}_d{depth}.{mode_name}.{policy}.out"
                parsed = parse_output(r.stdout)
                if r.returncode == 0 or not parsed[1]:
                    out_path.write_text(r.stdout)
                    failures.append((case_id, trap, f"{mode_name}/{policy}/no-trap-or-empty-stack", r.stdout, wat_path))
                    continue
                if policy == "instruction":
                    baseline = parsed
                    baseline_out = out_path
                    baseline_stdout = r.stdout
                elif baseline is not None and parsed != baseline:
                    if baseline_stdout is not None and baseline_out is not None:
                        baseline_out.write_text(baseline_stdout)
                    out_path.write_text(r.stdout)
                    failures.append(
                        (
                            case_id,
                            trap,
                            f"{mode_name}/{policy}/mismatch baseline={baseline} actual={parsed} baseline_out={baseline_out} out={out_path}",
                            r.stdout,
                            wat_path,
                        )
                    )

        if len(failures) == case_failure_count:
            wat_path.unlink(missing_ok=True)
            wasm_path.unlink(missing_ok=True)

        if (case_id + 1) % 8 == 0:
            print(f"[fuzz] completed {case_id + 1}/{ns.cases} cases")

    if failures:
        print(f"[fuzz] FAIL failures={len(failures)} total_runs={total_runs}")
        for case_id, trap, where, output, wat_path in failures[:10]:
            print(f"[fuzz] failure case={case_id} trap={trap} where={where} wat={wat_path}")
            print(output[:1200])
        return 1

    shutil.rmtree(work, ignore_errors=True)
    print(f"[fuzz] PASS cases={ns.cases} total_runs={total_runs}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

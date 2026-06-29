#!/usr/bin/env python3
"""P3 regression guard for uwvm --mode section-details."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path


WASM_HEADER = b"\0asm\x01\0\0\0"
PERF_EVENTS = (
    "cycles,instructions,branches,branch-misses,"
    "L1-dcache-loads,L1-dcache-load-misses"
)


@dataclass(frozen=True)
class Case:
    name: str
    path: Path
    feature_1p1: bool = False
    benchmark: bool = True


def run(
    args: list[str],
    *,
    cwd: Path | None = None,
    stdout=None,
    stderr=None,
    check: bool = True,
) -> subprocess.CompletedProcess:
    return subprocess.run(args, cwd=cwd, stdout=stdout, stderr=stderr, check=check)


def leb_u32(value: int) -> bytes:
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            out.append(byte | 0x80)
        else:
            out.append(byte)
            return bytes(out)


def section(section_id: int, payload: bytes) -> bytes:
    return bytes([section_id]) + leb_u32(len(payload)) + payload


def write_wasm_from_wat(wat: str, out: Path, *wat2wasm_args: str) -> None:
    tool = shutil.which("wat2wasm")
    if tool is None:
        raise RuntimeError("wat2wasm is required to generate text-format guard cases")
    proc = subprocess.run(
        [tool, *wat2wasm_args, "-", "-o", str(out)],
        input=wat.encode(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.decode(errors="replace"))


def generate_cases(work_dir: Path) -> list[Case]:
    cases_dir = work_dir / "cases"
    cases_dir.mkdir(parents=True, exist_ok=True)

    empty = cases_dir / "empty.wasm"
    empty.write_bytes(WASM_HEADER)

    mixed = cases_dir / "mixed_mvp.wasm"
    write_wasm_from_wat(
        """
        (module
          (type (func (param i32) (result i32)))
          (type (func))
          (func (type 0) local.get 0)
          (func (type 1))
          (table 2 funcref)
          (memory 1 2)
          (global (mut i32) (i32.const 7))
          (export "f" (func 0))
          (export "mem" (memory 0))
          (export "tab" (table 0))
          (export "g" (global 0))
          (start 1)
          (elem (i32.const 0) func 0)
          (data (i32.const 0) "abc"))
        """,
        mixed,
    )

    imports = cases_dir / "imports_mvp.wasm"
    write_wasm_from_wat(
        """
        (module
          (type (func (param i32) (result i32)))
          (import "env" "imp_func" (func (type 0)))
          (import "env" "imp_table" (table 2 funcref))
          (import "env" "imp_mem" (memory 1 2))
          (import "env" "imp_global" (global i32))
          (export "if" (func 0))
          (export "imem" (memory 0))
          (export "itab" (table 0))
          (export "ig" (global 0)))
        """,
        imports,
    )

    data_count = cases_dir / "wasm1p1_data_count.wasm"
    write_wasm_from_wat(
        '(module (memory 1) (data "bulk-data") (func (data.drop 0)))',
        data_count,
    )

    functions_70000 = cases_dir / "functions_70000.wasm"
    if not functions_70000.exists():
        wat = ["(module (type (func))"]
        wat.extend("  (func)" for _ in range(70000))
        wat.append(")")
        write_wasm_from_wat("\n".join(wat), functions_70000)

    huge_custom = cases_dir / "huge_custom_1m.wasm"
    custom_name = b"huge_custom"
    custom_payload = leb_u32(len(custom_name)) + custom_name + (b"x" * (1024 * 1024))
    huge_custom.write_bytes(WASM_HEADER + section(0, custom_payload))

    huge_data = cases_dir / "huge_data_1m.wasm"
    mem_payload = leb_u32(1) + bytes([0]) + leb_u32(16)
    init_expr = bytes([0x41]) + leb_u32(0) + bytes([0x0B])
    data = b"d" * (1024 * 1024)
    data_payload = leb_u32(1) + bytes([0]) + init_expr + leb_u32(len(data)) + data
    huge_data.write_bytes(WASM_HEADER + section(5, mem_payload) + section(11, data_payload))

    huge_code = cases_dir / "huge_code_1m.wasm"
    type_payload = leb_u32(1) + bytes([0x60, 0x00, 0x00])
    func_payload = leb_u32(1) + leb_u32(0)
    body = bytes([0]) + (b"\x01" * (1024 * 1024)) + bytes([0x0B])
    code_payload = leb_u32(1) + leb_u32(len(body)) + body
    huge_code.write_bytes(
        WASM_HEADER
        + section(1, type_payload)
        + section(3, func_payload)
        + section(10, code_payload)
    )

    return [
        Case("empty", empty, benchmark=False),
        Case("mixed_mvp", mixed),
        Case("imports_mvp", imports),
        Case("wasm1p1_data_count", data_count, feature_1p1=True),
        Case("functions_70000", functions_70000),
        Case("huge_custom_1m", huge_custom, benchmark=False),
        Case("huge_data_1m", huge_data, benchmark=False),
        Case("huge_code_1m", huge_code, benchmark=False),
    ]


def configure_and_build(root: Path, stack_usage: bool) -> tuple[float, str]:
    configure = ["xmake", "f", "-c", "-m", "release", "--use-llvm-compiler=y"]
    if stack_usage:
        configure.extend(
            [
                "--march=none",
                "--enable-lto=n",
                "--cxxflags=-fstack-usage",
                "--cxflags=-fstack-usage",
            ]
        )
    run(configure, cwd=root)

    start = time.perf_counter()
    proc = run(["xmake", "-r"], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    elapsed = time.perf_counter() - start
    return elapsed, proc.stdout.decode(errors="replace")


def binary_path(root: Path) -> Path:
    path = root / "build/linux/x86_64/release/uwvm"
    if not path.exists():
        raise FileNotFoundError(f"missing release uwvm binary: {path}")
    return path


def command(binary: Path, case: Case) -> list[str]:
    args = [str(binary)]
    if case.feature_1p1:
        args.append("--wasm-feature-1p1")
    args.extend(["--mode", "section-details", str(case.path)])
    return args


SPAN_RE = re.compile(
    rb"Module span: 0x[0-9a-fA-F]+ - 0x[0-9a-fA-F]+ \(length=([0-9]+)\)"
)


def normalize_output(data: bytes) -> bytes:
    return SPAN_RE.sub(rb"Module span: <normalized> (length=\1)", data)


def compare_outputs(old_bin: Path, new_bin: Path, cases: list[Case], out_dir: Path) -> list[str]:
    out_dir.mkdir(parents=True, exist_ok=True)
    lines: list[str] = []
    for case in cases:
        old = run(command(old_bin, case), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        new = run(command(new_bin, case), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        old_norm = normalize_output(old.stdout)
        new_norm = normalize_output(new.stdout)
        (out_dir / f"{case.name}.old.out").write_bytes(old.stdout)
        (out_dir / f"{case.name}.new.out").write_bytes(new.stdout)
        if old.stderr or new.stderr:
            raise RuntimeError(f"{case.name}: unexpected stderr in output comparison")
        if old_norm != new_norm:
            raise RuntimeError(f"{case.name}: normalized old/new output differs")
        lines.append(f"{case.name}: normalized_cmp=ok stdout={len(new.stdout)}")
    return lines


def timed_run(binary: Path, case: Case) -> float:
    start = time.perf_counter()
    run(command(binary, case), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return time.perf_counter() - start


def stats(values: list[float]) -> dict[str, float]:
    return {
        "n": float(len(values)),
        "mean": statistics.fmean(values),
        "median": statistics.median(values),
        "min": min(values),
        "max": max(values),
        "stdev": statistics.pstdev(values),
    }


def benchmark(old_bin: Path, new_bin: Path, cases: list[Case], runs: int, warmup: int) -> list[str]:
    lines: list[str] = []
    for case in cases:
        if not case.benchmark:
            continue
        for _ in range(warmup):
            timed_run(old_bin, case)
            timed_run(new_bin, case)
        old_times: list[float] = []
        new_times: list[float] = []
        for _ in range(runs):
            old_times.append(timed_run(old_bin, case))
            new_times.append(timed_run(new_bin, case))
        old = stats(old_times)
        new = stats(new_times)
        lines.append(
            f"{case.name}: old_mean={old['mean']:.9f}s new_mean={new['mean']:.9f}s "
            f"mean_speedup={old['mean'] / new['mean']:.6f}x "
            f"old_median={old['median']:.9f}s new_median={new['median']:.9f}s "
            f"median_speedup={old['median'] / new['median']:.6f}x"
        )
    return lines


def syscall_counts(binary: Path, case: Case, log_path: Path) -> str:
    if shutil.which("strace") is None:
        return f"{case.name}: strace=skipped"
    proc = subprocess.run(
        ["strace", "-qq", "-e", "trace=write,writev", "-c", *command(binary, case)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        check=False,
    )
    log_path.write_bytes(proc.stderr)
    text = proc.stderr.decode(errors="replace")
    if proc.returncode != 0:
        return f"{case.name}: strace=failed returncode={proc.returncode}"
    counts: dict[str, int] = {}
    for line in text.splitlines():
        parts = line.split()
        if parts and parts[-1] in {"write", "writev"}:
            counts[parts[-1]] = int(parts[3])
    total = sum(counts.values())
    return f"{case.name}: total={total} write={counts.get('write', 0)} writev={counts.get('writev', 0)}"


def stack_usage(root: Path) -> list[str]:
    rows: list[tuple[int, str]] = []
    precise: list[tuple[int, str]] = []
    patterns = (
        "print_context_define",
        "section_print",
        "emit_literal",
        "emit_reserve",
        "emit_dynamic_reserve",
        "body_context",
    )
    for path in (root / "build").rglob("*.su"):
        for line in path.read_text(errors="ignore").splitlines():
            parts = line.split("\t")
            if len(parts) < 2:
                continue
            try:
                size = int(parts[1])
            except ValueError:
                continue
            rows.append((size, parts[0]))
            if any(pattern in parts[0] for pattern in patterns):
                precise.append((size, parts[0]))
    rows.sort(reverse=True)
    precise.sort(reverse=True)
    all_max = rows[0][0] if rows else 0
    precise_max = precise[0][0] if precise else 0
    print_controls = [row for row in rows if "print_controls_impl" in row[1]]
    print_controls_max = print_controls[0][0] if print_controls else 0
    lines = [
        f"all_max={all_max}",
        f"context_precise_max={precise_max}",
        f"print_controls_impl_max={print_controls_max}",
        "top20:",
    ]
    lines.extend(f"{size}\t{name}" for size, name in rows[:20])
    lines.append("context_top20:")
    lines.extend(f"{size}\t{name}" for size, name in precise[:20])
    return lines


def perf_stat(binary: Path, case: Case, repetitions: int, log_path: Path) -> str:
    if shutil.which("perf") is None:
        return f"{case.name}: perf=skipped"
    proc = subprocess.run(
        [
            "perf",
            "stat",
            "-r",
            str(repetitions),
            "-e",
            PERF_EVENTS,
            *command(binary, case),
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        check=False,
    )
    log_path.write_bytes(proc.stderr)
    if proc.returncode != 0:
        reason = "unknown"
        for line in proc.stderr.decode(errors="replace").splitlines():
            line = line.strip()
            if line and line != "Error:":
                reason = line
                break
        return f"{case.name}: perf=failed returncode={proc.returncode} reason={reason}"
    return f"{case.name}: perf=ok"


def write_lines(path: Path, lines: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--old-root", type=Path, required=True)
    parser.add_argument("--new-root", type=Path, default=Path("."))
    parser.add_argument("--work-dir", type=Path, default=Path("/tmp/uwvm2_p3_guard"))
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--stack-usage", action="store_true")
    parser.add_argument("--perf", action="store_true")
    parser.add_argument("--perf-repetitions", type=int, default=30)
    parser.add_argument("--runs", type=int, default=30)
    parser.add_argument("--warmup", type=int, default=6)
    ns = parser.parse_args()

    old_root = ns.old_root.resolve()
    new_root = ns.new_root.resolve()
    work_dir = ns.work_dir.resolve()
    work_dir.mkdir(parents=True, exist_ok=True)

    summary: list[str] = [
        "P3 section-details guard",
        f"old_root={old_root}",
        f"new_root={new_root}",
        f"work_dir={work_dir}",
    ]

    if not ns.skip_build:
        for label, root in (("old", old_root), ("new", new_root)):
            elapsed, build_log = configure_and_build(root, ns.stack_usage)
            (work_dir / f"{label}.build.log").write_text(build_log)
            summary.append(f"{label}_build_elapsed={elapsed:.3f}s")

    old_bin = binary_path(old_root)
    new_bin = binary_path(new_root)
    summary.append(f"old_binary_size={old_bin.stat().st_size}")
    summary.append(f"new_binary_size={new_bin.stat().st_size}")

    cases = generate_cases(work_dir)
    summary.append("cases:")
    summary.extend(f"  {case.name} path={case.path} feature_1p1={case.feature_1p1}" for case in cases)

    cmp_lines = compare_outputs(old_bin, new_bin, cases, work_dir / "outputs")
    write_lines(work_dir / "compare.summary", cmp_lines)
    summary.append("compare=ok")

    bench_lines = benchmark(old_bin, new_bin, cases, ns.runs, ns.warmup)
    write_lines(work_dir / "benchmark.summary", bench_lines)
    summary.append("benchmark=ok")

    syscall_lines: list[str] = ["old:"]
    syscall_lines.extend(
        syscall_counts(old_bin, case, work_dir / f"old.{case.name}.strace")
        for case in cases
        if case.benchmark
    )
    syscall_lines.append("new:")
    syscall_lines.extend(
        syscall_counts(new_bin, case, work_dir / f"new.{case.name}.strace")
        for case in cases
        if case.benchmark
    )
    write_lines(work_dir / "write_count.summary", syscall_lines)
    summary.append("write_count=ok")

    if ns.stack_usage:
        write_lines(work_dir / "old.stack_usage.summary", stack_usage(old_root))
        write_lines(work_dir / "new.stack_usage.summary", stack_usage(new_root))
        summary.append("stack_usage=ok")

    if ns.perf:
        perf_lines = ["old:"]
        perf_lines.extend(
            perf_stat(old_bin, case, ns.perf_repetitions, work_dir / f"old.{case.name}.perf")
            for case in cases
            if case.benchmark
        )
        perf_lines.append("new:")
        perf_lines.extend(
            perf_stat(new_bin, case, ns.perf_repetitions, work_dir / f"new.{case.name}.perf")
            for case in cases
            if case.benchmark
        )
        write_lines(work_dir / "perf.summary", perf_lines)
        summary.append("perf=done")

    write_lines(work_dir / "summary.txt", summary)
    print("\n".join(summary))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"p3_guard.py: error: {exc}", file=sys.stderr)
        raise SystemExit(1)

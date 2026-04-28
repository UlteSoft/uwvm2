#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


@dataclass(frozen=True)
class Case:
    name: str
    main_wasm: str
    main_module_name: str
    relay_wasm: str | None = None
    relay_module_name: str | None = None


ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _case_root() -> Path:
    return Path(__file__).resolve().parent


def _compile_wat(case_root: Path) -> None:
    script = case_root / "compile_wat.py"
    subprocess.run([sys.executable, str(script)], cwd=case_root, check=True)


def _xmake_show_uwvm_targetfile(repo_root: Path) -> Path:
    proc = subprocess.run(
        ["xmake", "show", "-t", "uwvm"],
        cwd=repo_root,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    clean = ANSI_RE.sub("", proc.stdout)
    for line in clean.splitlines():
        if "targetfile:" in line:
            return (repo_root / line.split("targetfile:", 1)[1].strip()).resolve()
    raise RuntimeError("could not locate uwvm targetfile from `xmake show -t uwvm`")


def _build_uwvm(repo_root: Path) -> Path:
    subprocess.run(["xmake", "build", "uwvm"], cwd=repo_root, check=True)
    targetfile = _xmake_show_uwvm_targetfile(repo_root)
    if not targetfile.is_file():
        raise RuntimeError(f"uwvm targetfile does not exist: {targetfile}")
    return targetfile


def _uwvm_args(uwvm_bin: Path, case: Case) -> list[str]:
    args = [
        str(uwvm_bin),
        "--runtime-custom-mode",
        "full",
        "--runtime-custom-compiler",
        "int",
    ]

    if case.relay_wasm is not None and case.relay_module_name is not None:
        args.extend(
            [
                "--wasm-preload-library",
                str(case.relay_wasm),
                case.relay_module_name,
            ]
        )

    args.extend(
        [
            "--wasm-set-main-module-name",
            case.main_module_name,
            "--run",
            str(case.main_wasm),
        ]
    )
    return args


def _run_case(uwvm_bin: Path, case: Case) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        _uwvm_args(uwvm_bin, case),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def _measure_case(uwvm_bin: Path, case: Case, warmups: int, runs: int) -> list[float]:
    for _ in range(warmups):
        proc = _run_case(uwvm_bin, case)
        if proc.returncode != 0:
            raise RuntimeError(f"warmup failed for {case.name}:\n{proc.stderr}")

    samples: list[float] = []
    for _ in range(runs):
        start = time.perf_counter()
        proc = _run_case(uwvm_bin, case)
        elapsed = time.perf_counter() - start
        if proc.returncode != 0:
            raise RuntimeError(f"benchmark run failed for {case.name}:\n{proc.stderr}")
        samples.append(elapsed)
    return samples


def _mean(values: Sequence[float]) -> float:
    return sum(values) / len(values)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run correctness and benchmark checks for the uwvm-int wasm import fast path.")
    parser.add_argument("--bench-runs", type=int, default=3, help="Benchmark repetitions per case (default: 3)")
    parser.add_argument("--warmup-runs", type=int, default=1, help="Warmup runs per benchmark case (default: 1)")
    parser.add_argument("--skip-bench", action="store_true", help="Only run correctness checks")
    args = parser.parse_args()

    repo_root = _repo_root()
    case_root = _case_root()
    wat_dir = case_root / "wat"

    _compile_wat(case_root)
    uwvm_bin = _build_uwvm(repo_root)

    smoke_local = Case(
        name="smoke.local",
        main_wasm=str((wat_dir / "call_fastpath_local_smoke_main.wasm").resolve()),
        main_module_name="call_fastpath_local_main",
    )
    smoke_cycle = Case(
        name="smoke.cycle",
        main_wasm=str((wat_dir / "call_fastpath_cycle_smoke_main.wasm").resolve()),
        main_module_name="call_fastpath_cycle_main",
        relay_wasm=str((wat_dir / "call_fastpath_cycle_relay.wasm").resolve()),
        relay_module_name="call_fastpath_cycle_relay",
    )
    bench_local = Case(
        name="bench.local",
        main_wasm=str((wat_dir / "call_fastpath_local_bench_main.wasm").resolve()),
        main_module_name="call_fastpath_local_main",
    )
    bench_cycle = Case(
        name="bench.cycle",
        main_wasm=str((wat_dir / "call_fastpath_cycle_bench_main.wasm").resolve()),
        main_module_name="call_fastpath_cycle_main",
        relay_wasm=str((wat_dir / "call_fastpath_cycle_relay.wasm").resolve()),
        relay_module_name="call_fastpath_cycle_relay",
    )

    failed = 0
    for case in (smoke_local, smoke_cycle):
        proc = _run_case(uwvm_bin, case)
        if proc.returncode != 0:
            failed += 1
            sys.stderr.write(f"[FAIL] {case.name}: returncode={proc.returncode}\n")
            if proc.stdout:
                sys.stderr.write("---- stdout ----\n")
                sys.stderr.write(proc.stdout)
                if not proc.stdout.endswith("\n"):
                    sys.stderr.write("\n")
            if proc.stderr:
                sys.stderr.write("---- stderr ----\n")
                sys.stderr.write(proc.stderr)
                if not proc.stderr.endswith("\n"):
                    sys.stderr.write("\n")
        else:
            sys.stdout.write(f"[OK] {case.name}\n")

    if failed:
        return 1

    if args.skip_bench:
        return 0

    local_samples = _measure_case(uwvm_bin, bench_local, args.warmup_runs, args.bench_runs)
    cycle_samples = _measure_case(uwvm_bin, bench_cycle, args.warmup_runs, args.bench_runs)

    local_avg = _mean(local_samples)
    cycle_avg = _mean(cycle_samples)
    overhead = ((cycle_avg / local_avg) - 1.0) * 100.0 if local_avg != 0.0 else 0.0

    sys.stdout.write(
        f"[BENCH] local avg={local_avg:.6f}s samples={','.join(f'{v:.6f}' for v in local_samples)}\n"
    )
    sys.stdout.write(
        f"[BENCH] cycle avg={cycle_avg:.6f}s samples={','.join(f'{v:.6f}' for v in cycle_samples)}\n"
    )
    sys.stdout.write(f"[BENCH] cycle_vs_local_overhead={overhead:.2f}%\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

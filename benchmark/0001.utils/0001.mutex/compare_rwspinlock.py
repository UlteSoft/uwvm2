#!/usr/bin/env python3
from __future__ import annotations

import os
import re
import shlex
import subprocess
import sys
from pathlib import Path


def run_or_die(argv: list[str], *, cwd: Path | None = None, env: dict[str, str] | None = None, stdout_path: Path | None = None, append: bool = False) -> None:
    print(">> " + " ".join(shlex.quote(x) for x in argv))
    stdout_file = None
    try:
        if stdout_path is not None:
            stdout_path.parent.mkdir(parents=True, exist_ok=True)
            stdout_file = open(stdout_path, "ab" if append else "wb")
            subprocess.run(argv, cwd=cwd, env=env, stdout=stdout_file, stderr=subprocess.STDOUT, check=True)
        else:
            subprocess.run(argv, cwd=cwd, env=env, check=True)
    finally:
        if stdout_file is not None:
            stdout_file.close()


def simd_flags(level: str) -> list[str]:
    if level == "native":
        return ["-march=native"]
    if level == "sse2":
        return ["-msse2"]
    if level == "sse3":
        return ["-msse3"]
    if level == "ssse3":
        return ["-mssse3"]
    if level == "sse4":
        return ["-msse4.1", "-msse4.2"]
    if level == "avx":
        return ["-mavx"]
    if level == "avx2":
        return ["-mavx2"]
    if level == "avx512bw":
        return ["-mavx512bw"]
    if level == "avx512vbmi":
        return ["-mavx512bw", "-mavx512vbmi", "-mavx512vbmi2"]
    return ["-march=native"]


def parse_timer_log(log_path: Path) -> list[tuple[str, float]]:
    timers: list[tuple[str, float]] = []
    if not log_path.exists():
        return timers
    pattern = re.compile(r'timer "([^"]+)":\s*([\d.]+)s')
    for line in log_path.read_text(errors="replace").splitlines():
        m = pattern.search(line)
        if not m:
            continue
        name = m.group(1)
        seconds = float(m.group(2))
        timers.append((name, seconds))
    timers.sort(key=lambda t: t[0])
    return timers


def main() -> int:
    script_path = Path(__file__).resolve()
    script_dir = script_path.parent
    root_dir = script_dir.parents[2]

    output_dir = script_dir / "outputs"
    output_dir.mkdir(parents=True, exist_ok=True)

    bench_src = root_dir / "benchmark/0001.utils/0001.mutex/RWSpinLock.cc"
    bench_bin = output_dir / "RWSpinLock"
    bench_log = output_dir / "rwspinlock_bench.log"
    bench_summary = output_dir / "rwspinlock_summary.txt"

    print("uwvm2 utils::mutex rwlock_t vs bench_folly::RWSpinLock benchmark (Python driver)")
    print(f"  script_dir = {script_dir}")
    print(f"  root_dir   = {root_dir}")
    print(f"  output_dir = {output_dir}")

    print("==> Building RWSpinLock benchmark (C++)")
    cxx = os.environ.get("CXX", "clang++")
    extra_flags = shlex.split(os.environ.get("CXXFLAGS_EXTRA", ""))
    simd_level = os.environ.get("UWVM2_SIMD_LEVEL", "native")

    default_flags = [
        "-std=c++2c",
        "-O3",
        "-ffast-math",
        "-fno-rtti",
        "-fno-unwind-tables",
        "-fno-asynchronous-unwind-tables",
        *simd_flags(simd_level),
        *extra_flags,
    ]

    include_flags = [
        "-I",
        str(root_dir / "src"),
        "-I",
        str(root_dir / "third-parties/fast_io/include"),
        "-I",
        str(root_dir / "third-parties/bizwen/include"),
        "-I",
        str(root_dir / "third-parties/boost_unordered/include"),
    ]

    run_or_die(
        [cxx, *default_flags, *include_flags, str(bench_src), "-o", str(bench_bin)],
        cwd=root_dir,
    )

    print("==> Running RWSpinLock benchmark")
    run_or_die([str(bench_bin)], cwd=root_dir, stdout_path=bench_log, append=False)

    timers = parse_timer_log(bench_log)
    if timers:
        bench_summary.write_text("".join(f"{name}: {seconds} s\n" for name, seconds in timers))

    print()
    print("Timer results (seconds):")
    if not timers:
        print("  <no timer lines parsed; see log file for raw output>")
        print(f"  log: {bench_log}")
    else:
        for name, seconds in timers:
            print(f"  {name:<32} {seconds}")
        print()
        print(f"Summary written to: {bench_summary}")
        print(f"Raw log saved to : {bench_log}")

    print()
    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


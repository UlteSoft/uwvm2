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


def ensure_simdutf_repo(*, output_dir: Path) -> Path:
    env_dir = os.environ.get("SIMDUTF_DIR")
    if env_dir:
        simdutf_dir = Path(env_dir).expanduser().resolve()
        if not simdutf_dir.exists():
            raise SystemExit(f"SIMDUTF_DIR is set but directory does not exist: {simdutf_dir}")
        return simdutf_dir

    default_dir = output_dir / "simdutf"
    if default_dir.exists():
        return default_dir.resolve()

    print(f"==> Cloning simdutf into {default_dir}")
    run_or_die(["git", "clone", "--depth", "1", "https://github.com/simdutf/simdutf.git", str(default_dir)])
    return default_dir.resolve()


def detect_simdutf_include(simdutf_dir: Path) -> Path:
    env_inc = os.environ.get("SIMDUTF_INCLUDE")
    if env_inc:
        inc = Path(env_inc).expanduser().resolve()
        if not inc.exists():
            raise SystemExit(f"SIMDUTF_INCLUDE is set but directory does not exist: {inc}")
        return inc

    if (simdutf_dir / "singleheader/simdutf.h").exists():
        return (simdutf_dir / "singleheader").resolve()
    if (simdutf_dir / "include/simdutf.h").exists():
        return (simdutf_dir / "include").resolve()
    raise SystemExit(f"could not locate simdutf.h under {simdutf_dir} (tried singleheader/ and include/)")


def main() -> int:
    script_path = Path(__file__).resolve()
    script_dir = script_path.parent
    root_dir = script_dir.parents[2]

    output_dir = script_dir / "outputs"
    output_dir.mkdir(parents=True, exist_ok=True)

    gen_src = root_dir / "benchmark/0001.utils/0002.utf/UtfGen.cc"
    gen_bin = output_dir / "UtfGen"

    uwvm2_src = root_dir / "benchmark/0001.utils/0002.utf/UtfUwvm2Bench.cc"
    uwvm2_bin = output_dir / "UtfUwvm2Bench"

    simdutf_src = root_dir / "benchmark/0001.utils/0002.utf/UtfSimdutfBench.cc"
    simdutf_bin = output_dir / "UtfSimdutfBench"

    bench_log = output_dir / "utf_bench.log"
    bench_summary = output_dir / "utf_bench_summary.txt"

    print("uwvm2 UTF-8 verification benchmark (uwvm2 vs simdutf, Python driver)")
    print(f"  script_dir = {script_dir}")
    print(f"  root_dir   = {root_dir}")
    print(f"  output_dir = {output_dir}")

    simdutf_dir = ensure_simdutf_repo(output_dir=output_dir)
    simdutf_inc = detect_simdutf_include(simdutf_dir)
    print(f"  simdutf_dir= {simdutf_dir}")

    print("==> Building uwvm2 vs simdutf UTF-8 benchmarks (C++)")
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

    simdutf_impl = simdutf_dir / "src/simdutf.cpp"
    if not simdutf_impl.exists():
        raise SystemExit(f"simdutf implementation source not found: {simdutf_impl}")

    def compile_one(sources: list[Path], out_bin: Path, extra: list[str] | None = None) -> None:
        argv = [cxx, *default_flags, *include_flags]
        if extra:
            argv.extend(extra)
        argv.extend(str(s) for s in sources)
        argv.extend(["-o", str(out_bin)])
        run_or_die(argv, cwd=root_dir)

    compile_one([gen_src], gen_bin)
    compile_one([uwvm2_src], uwvm2_bin)
    compile_one(
        [simdutf_src, simdutf_impl],
        simdutf_bin,
        extra=[
            "-I",
            str(simdutf_inc),
            "-I",
            str(simdutf_dir / "src"),
            "-DSIMDUTF_CPLUSPLUS17=0",
            "-DSIMDUTF_CPLUSPLUS20=0",
            "-DSIMDUTF_CPLUSPLUS23=0",
        ],
    )

    print("==> Running UTF-8 benchmark")
    bench_log.write_bytes(b"")

    def base_env() -> dict[str, str]:
        env = dict(os.environ)
        for k in ["UTF_BENCH_BYTES", "ITERS", "WARMUP_ITERS", "TRIALS", "RUSTFLAGS"]:
            v = os.environ.get(k)
            if v:
                env[k] = v
        return env

    forced_impl = os.environ.get("SIMDUTF_FORCE_IMPLEMENTATION", "")
    simdutf_forced_env: dict[str, str] = {}
    if not forced_impl and simd_level != "native":
        impl_map = {
            "sse2": "fallback",
            "sse3": "fallback",
            "ssse3": "fallback",
            "sse4": "westmere",
            "avx": "westmere",
            "avx2": "haswell",
            "avx512bw": "icelake",
            "avx512vbmi": "icelake",
        }
        impl = impl_map.get(simd_level)
        if impl:
            simdutf_forced_env["SIMDUTF_FORCE_IMPLEMENTATION"] = impl
            print(f"  forcing simdutf implementation={impl} (UWVM2_SIMD_LEVEL={simd_level})")

    scenarios = ["ascii_only", "latin_mixed", "emoji_mixed"]
    for i, scenario in enumerate(scenarios, start=1):
        data_file = output_dir / f"utf_data_{scenario}.bin"

        env_common = base_env()
        env_common["SCENARIO"] = scenario
        env_common["DATA_FILE"] = str(data_file)

        run_or_die([str(gen_bin)], cwd=root_dir, env=env_common, stdout_path=bench_log, append=True)

        env_uwvm = dict(env_common)
        env_simd = dict(env_common)
        env_simd.update(simdutf_forced_env)

        if (i % 2) == 1:
            run_or_die([str(uwvm2_bin)], cwd=root_dir, env=env_uwvm, stdout_path=bench_log, append=True)
            run_or_die([str(simdutf_bin)], cwd=root_dir, env=env_simd, stdout_path=bench_log, append=True)
        else:
            run_or_die([str(simdutf_bin)], cwd=root_dir, env=env_simd, stdout_path=bench_log, append=True)
            run_or_die([str(uwvm2_bin)], cwd=root_dir, env=env_uwvm, stdout_path=bench_log, append=True)

    uwvm: dict[str, float] = {}
    simd: dict[str, float] = {}
    for line in bench_log.read_text(errors="replace").splitlines():
        if not line.startswith("uwvm2_utf "):
            continue
        scenario = re.search(r"scenario=([^\s]+)", line)
        impl = re.search(r"impl=([^\s]+)", line)
        ns = re.search(r"ns_per_byte=([^\s]+)", line)
        if not (scenario and impl and ns):
            continue
        try:
            val = float(ns.group(1))
        except ValueError:
            continue
        if impl.group(1) == "uwvm2":
            uwvm[scenario.group(1)] = val
        elif impl.group(1) == "simdutf":
            simd[scenario.group(1)] = val

    summary_lines: list[str] = []
    for scenario, ns in uwvm.items():
        summary_lines.append(f"scenario={scenario} impl=uwvm2 ns_per_byte={ns:.6f}")
    for scenario, ns in simd.items():
        summary_lines.append(f"scenario={scenario} impl=simdutf ns_per_byte={ns:.6f}")
    summary_lines.sort()
    if summary_lines:
        bench_summary.write_text("\n".join(summary_lines) + "\n")

    print()
    print("=" * 80)
    print("Per-scenario comparison (ns/byte, smaller is faster)")
    print("=" * 80)
    for scenario in sorted(uwvm.keys()):
        ns_uwvm = uwvm[scenario]
        ns_simd = simd.get(scenario)
        print()
        print(f"Scenario: {scenario}")
        print(f"  uwvm2  : ns_per_byte = {ns_uwvm:.6f}")
        if ns_simd is None:
            print("  simdutf: <no result recorded>")
        else:
            print(f"  simdutf: ns_per_byte = {ns_simd:.6f}")
            print(f"  ratio  : uwvm2 / simdutf ≈ {ns_uwvm / ns_simd:.3f}  (<1 means uwvm2 is faster)")

    print()
    print(f"Raw log saved to: {bench_log}")
    print(f"Summary written to: {bench_summary}")
    print()
    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


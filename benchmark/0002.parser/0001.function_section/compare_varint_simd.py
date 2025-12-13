#!/usr/bin/env python3
from __future__ import annotations

import os
import re
import shlex
import subprocess
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


def ensure_varint_repo(*, output_dir: Path) -> Path:
    default_dir = output_dir / "varint-simd"

    env_dir = os.environ.get("VARINT_SIMD_DIR")
    if env_dir:
        src = Path(env_dir).expanduser().resolve()
        if not src.exists():
            raise SystemExit(f"VARINT_SIMD_DIR is set but directory does not exist: {src}")
        if default_dir.exists():
            return default_dir.resolve()
        try:
            default_dir.parent.mkdir(parents=True, exist_ok=True)
            default_dir.symlink_to(src)
        except Exception as e:  # noqa: BLE001
            raise SystemExit(
                "VARINT_SIMD_DIR is set, but cannot create symlink at "
                f"{default_dir} -> {src} ({e}). "
                "Please clone/copy the repository into outputs/varint-simd instead."
            ) from e
        return default_dir.resolve()

    if default_dir.exists():
        return default_dir.resolve()

    print(f"==> Cloning varint-simd into {default_dir}")
    run_or_die(["git", "clone", "--depth", "1", "https://github.com/as-com/varint-simd.git", str(default_dir)])
    return default_dir.resolve()


def rustflags_for_simd_level(level: str) -> str:
    if level == "native":
        extra = "-C target-cpu=native"
    else:
        features_map = {
            "sse2": "+sse2",
            "sse3": "+sse2,+sse3",
            "ssse3": "+sse2,+sse3,+ssse3",
            "sse4": "+sse2,+sse3,+ssse3,+sse4.1,+sse4.2",
            "avx": "+sse2,+sse3,+ssse3,+sse4.1,+sse4.2,+avx",
            "avx2": "+sse2,+sse3,+ssse3,+sse4.1,+sse4.2,+avx,+avx2",
            "avx512bw": "+sse2,+sse3,+ssse3,+sse4.1,+sse4.2,+avx,+avx2,+avx512bw",
            "avx512vbmi": "+sse2,+sse3,+ssse3,+sse4.1,+sse4.2,+avx,+avx2,+avx512bw,+avx512vbmi,+avx512vbmi2",
        }
        features = features_map.get(level)
        extra = "-C target-cpu=native" if features is None else f"-C target-cpu=x86-64 -C target-feature={features}"

    base = os.environ.get("RUSTFLAGS", "").strip()
    return (base + " " + extra).strip() if base else extra


def main() -> int:
    script_path = Path(__file__).resolve()
    script_dir = script_path.parent
    root_dir = script_dir.parents[2]

    output_dir = script_dir / "outputs"
    output_dir.mkdir(parents=True, exist_ok=True)

    data_dir = output_dir / "data"
    data_dir.mkdir(parents=True, exist_ok=True)

    bench_src = root_dir / "benchmark/0002.parser/0001.function_section/FunctionSectionVarintSimd.cc"
    bench_bin = output_dir / "FunctionSectionVarintSimd"

    uwvm_log = output_dir / "uwvm2_uleb128_bench.log"
    uwvm_summary = output_dir / "uwvm2_uleb128_summary.txt"
    varint_log = output_dir / "varint_simd_bench.log"
    varint_summary = output_dir / "varint_simd_fair_summary.txt"

    print("uwvm2 function_section uleb128<u32> SIMD benchmark (varint-simd style, Python driver)")
    print(f"  script_dir = {script_dir}")
    print(f"  root_dir   = {root_dir}")
    print(f"  data_dir   = {data_dir}")

    print("==> Building uwvm2 FunctionSection uleb128 benchmark (C++)")
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

    print("==> Running uwvm2 benchmark")
    env_uwvm = dict(os.environ)
    if os.environ.get("FUNC_COUNT"):
        env_uwvm["FUNC_COUNT"] = os.environ["FUNC_COUNT"]
    if os.environ.get("ITERS"):
        env_uwvm["ITERS"] = os.environ["ITERS"]
    env_uwvm["FS_BENCH_DATA_DIR"] = str(data_dir)
    run_or_die([str(bench_bin)], cwd=root_dir, env=env_uwvm, stdout_path=uwvm_log, append=False)

    uwvm_lines = [line for line in uwvm_log.read_text(errors="replace").splitlines() if line.startswith("uwvm2_fs ")]
    if not uwvm_lines:
        raise SystemExit(f"no 'uwvm2_fs ' lines found in {uwvm_log}")
    uwvm_summary.write_text("\n".join(uwvm_lines) + "\n")

    print()
    print("uwvm2 per-scenario results (raw):")
    for line in uwvm_lines:
        print("  " + line)

    ensure_varint_repo(output_dir=output_dir)

    print()
    print("==> Running Rust varint-simd fair benchmark (shared data files)")
    fair_dir = script_dir / "varint-simd-fair"
    env_rust = dict(os.environ)
    env_rust["FS_BENCH_DATA_DIR"] = str(data_dir)
    env_rust["RUSTFLAGS"] = rustflags_for_simd_level(simd_level)
    env_rust["CARGO_TERM_COLOR"] = "never"
    run_or_die(["cargo", "run", "--release"], cwd=fair_dir, env=env_rust, stdout_path=varint_log, append=False)

    print("varint-simd raw output saved to:")
    print(f"  {varint_log}")

    unsafe: dict[str, float] = {}
    safe: dict[str, float] = {}
    for line in varint_log.read_text(errors="replace").splitlines():
        scenario = re.search(r"scenario=([^\s]+)", line)
        impl = re.search(r"impl=([^\s]+)", line)
        ns = re.search(r"ns_per_value=([^\s]+)", line)
        if not (scenario and impl and ns):
            continue
        try:
            val = float(ns.group(1))
        except ValueError:
            continue
        if impl.group(1) == "unsafe":
            unsafe[scenario.group(1)] = val
        elif impl.group(1) == "safe":
            safe[scenario.group(1)] = val

    summary_lines: list[str] = []
    for scenario, ns in unsafe.items():
        summary_lines.append(f"scenario={scenario} impl=unsafe ns_per_value={ns:.6f}")
    for scenario, ns in safe.items():
        summary_lines.append(f"scenario={scenario} impl=safe ns_per_value={ns:.6f}")
    summary_lines.sort()
    varint_summary.write_text(("\n".join(summary_lines) + "\n") if summary_lines else "")

    uwvm_simd_ns: dict[str, float] = {}
    for line in uwvm_summary.read_text(errors="replace").splitlines():
        scenario = re.search(r"scenario=([^\s]+)", line)
        impl = re.search(r"impl=([^\s]+)", line)
        ns = re.search(r"ns_per_value=([^\s]+)", line)
        if not (scenario and impl and ns):
            continue
        if impl.group(1) != "simd":
            continue
        try:
            uwvm_simd_ns[scenario.group(1)] = float(ns.group(1))
        except ValueError:
            continue

    def compare_one(scenario: str, desc: str) -> None:
        print()
        print("=" * 80)
        print(f"Scenario: {scenario}")
        print(f"  {desc}")

        ns_uwvm = uwvm_simd_ns.get(scenario)
        if ns_uwvm is None:
            print("  uwvm2 : <no SIMD result found>")
            return
        print(f"  uwvm2 : ns_per_value = {ns_uwvm:.6f}")

        ns_u = unsafe.get(scenario)
        if ns_u is None:
            print("  varint-simd (unsafe, fair-file) : <no unsafe result found>")
        else:
            print(f"  varint-simd (unsafe, fair-file) : ns_per_value ≈ {ns_u:.6f}")
            print(f"  ratio (unsafe)       : uwvm2 / varint-simd ≈ {ns_uwvm / ns_u:.3f}  ( <1 means uwvm2 is faster )")

        ns_s = safe.get(scenario)
        if ns_s is not None:
            print(f"  varint-simd (safe,   fair-file) : ns_per_value ≈ {ns_s:.6f}")

    print()
    print("=" * 80)
    print("Per-scenario comparison (ns/value, smaller is faster)")
    print("=" * 80)

    compare_one("u8_1b", "typeidx < 2^7, always 1-byte encoding (fast path, zero-copy u8 view)")
    compare_one("u8_2b", "2^7 ≤ typeidx < 2^8, 1–2 byte encodings, main fair comparison against varint-u8/decode")
    compare_one("u16_2b", "2^8 ≤ typeidx < 2^14, 1–2 byte encodings, rare in real-world wasm but useful as a stress case")

    print()
    print("=" * 80)
    print("Notes")
    print("=" * 80)
    print("  u8_1b  : specialized fast path (SIMD + zero-copy view); not a 1:1 fair decoder comparison.")
    print("  u8_2b  : main fair-comparison scenario (1–2 byte decode into array, vs varint-u8/decode).")
    print("  u16_2b : decodes into u16 array; rare in real wasm, closer to a stress/completeness case.")
    print()
    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


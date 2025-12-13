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


def ensure_xxhash_repo(*, output_dir: Path) -> Path:
    env_dir = os.environ.get("XXHASH_DIR")
    if env_dir:
        xxhash_dir = Path(env_dir).expanduser().resolve()
        if not xxhash_dir.exists():
            raise SystemExit(f"XXHASH_DIR is set but directory does not exist: {xxhash_dir}")
        return xxhash_dir

    default_dir = output_dir / "xxhash"
    if default_dir.exists():
        return default_dir.resolve()

    print(f"==> Cloning xxHash into {default_dir}")
    run_or_die(["git", "clone", "--depth", "1", "https://github.com/Cyan4973/xxHash.git", str(default_dir)])
    return default_dir.resolve()


def main() -> int:
    script_path = Path(__file__).resolve()
    script_dir = script_path.parent
    root_dir = script_dir.parents[2]

    output_dir = script_dir / "outputs"
    output_dir.mkdir(parents=True, exist_ok=True)

    gen_src = root_dir / "benchmark/0001.utils/0003.xxh3/Xxh3UtfGen.cc"
    gen_bin = output_dir / "Xxh3UtfGen"

    xxhash_src = root_dir / "benchmark/0001.utils/0003.xxh3/Xxh3UtfXxhashBench.cc"
    xxhash_bin = output_dir / "Xxh3UtfXxhashBench"

    uwvm2_src = root_dir / "benchmark/0001.utils/0003.xxh3/Xxh3UtfUwvm2Bench.cc"
    uwvm2_bin = output_dir / "Xxh3UtfUwvm2Bench"

    data_file = Path(os.environ.get("DATA_FILE", str(output_dir / "xxh3_utf_data.bin"))).expanduser()
    bench_log = output_dir / "xxh3_utf_bench.log"
    bench_summary = output_dir / "xxh3_utf_summary.txt"

    print("uwvm2 xxh3 vs upstream xxHash UTF benchmark (Python driver)")
    print(f"  script_dir = {script_dir}")
    print(f"  root_dir   = {root_dir}")
    print(f"  output_dir = {output_dir}")

    xxhash_dir = ensure_xxhash_repo(output_dir=output_dir)

    print("==> Building xxh3 UTF benchmarks (C++)")
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
        "-I",
        str(xxhash_dir),
    ]

    define_flags = ["-DXXH_INLINE_ALL", "-DXXH_STATIC_LINKING_ONLY"]

    def compile_one(src: Path, out_bin: Path, *, defines: list[str] | None = None) -> None:
        argv = [cxx, *default_flags]
        if defines:
            argv.extend(defines)
        argv.extend(include_flags)
        argv.extend([str(src), "-o", str(out_bin)])
        run_or_die(argv, cwd=root_dir)

    compile_one(gen_src, gen_bin)
    compile_one(xxhash_src, xxhash_bin, defines=define_flags)
    compile_one(uwvm2_src, uwvm2_bin)

    print("==> Running xxh3 UTF benchmarks")
    bench_log.write_bytes(b"")

    env_base = dict(os.environ)
    env_base["DATA_FILE"] = str(data_file)
    if os.environ.get("BYTES"):
        env_base["BYTES"] = os.environ["BYTES"]
    if os.environ.get("ITERS"):
        env_base["ITERS"] = os.environ["ITERS"]

    run_or_die([str(gen_bin)], cwd=root_dir, env=env_base, stdout_path=bench_log, append=True)
    run_or_die([str(xxhash_bin)], cwd=root_dir, env=env_base, stdout_path=bench_log, append=True)
    run_or_die([str(uwvm2_bin)], cwd=root_dir, env=env_base, stdout_path=bench_log, append=True)

    summary_lines = [line for line in bench_log.read_text(errors="replace").splitlines() if line.startswith("xxh3_utf ")]
    if not summary_lines:
        raise SystemExit(f"no 'xxh3_utf ' lines found in {bench_log}")
    bench_summary.write_text("\n".join(summary_lines) + "\n")

    print()
    print("Per-implementation throughput results (raw):")
    for line in summary_lines:
        print("  " + line)

    results: dict[str, float] = {}
    for line in summary_lines:
        impl = re.search(r"impl=([^\s]+)", line)
        gib = re.search(r"gib_per_s=([^\s]+)", line)
        if not (impl and gib):
            continue
        try:
            results[impl.group(1)] = float(gib.group(1))
        except ValueError:
            continue

    uwvm = results.get("uwvm2_xxh3")
    xxh = results.get("xxhash_xxh3")

    print()
    print("=" * 80)
    print("Throughput comparison (GiB/s, larger is better)")
    print("=" * 80)
    print("  uwvm2_xxh3 : " + ("<no result found>" if uwvm is None else f"{uwvm:.6f} GiB/s"))
    print("  xxhash_xxh3: " + ("<no result found>" if xxh is None else f"{xxh:.6f} GiB/s"))
    if uwvm is not None and xxh is not None:
        print()
        print(f"  ratio (uwvm2 / xxHash): {uwvm / xxh:.3f}  ( >1 means uwvm2 is faster )")

    print()
    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


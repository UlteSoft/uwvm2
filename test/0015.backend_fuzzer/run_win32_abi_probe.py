#!/usr/bin/env python3
"""Build and run the Windows ABI boundary probe used by backend fuzzer checks."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def quote_cmd(cmd: list[str]) -> str:
    return " ".join(cmd)


def compiler_accepts(cxx: str, flag: str) -> bool:
    proc = subprocess.run([cxx, flag, "-x", "c++", "-fsyntax-only", "-"], input="int main(){return 0;}\n", text=True)
    return proc.returncode == 0


def pick_std(cxx: str) -> str:
    for flag in ("-std=c++26", "-std=c++2c", "-std=c++23", "-std=c++20"):
        if compiler_accepts(cxx, flag):
            return flag
    raise SystemExit(f"{cxx} does not accept a usable C++ standard flag")


def cxx_candidates(explicit: str | None) -> list[str]:
    if explicit:
        return [explicit]
    env_cxx = os.environ.get("CXX")
    if sys.platform == "win32":
        candidates = [env_cxx, "clang++", "c++", "g++"]
    else:
        candidates = [env_cxx, "c++", "g++", "clang++"]
    out: list[str] = []
    for cxx in candidates:
        if not cxx:
            continue
        found = shutil.which(cxx) if not Path(cxx).exists() else cxx
        if found and str(found) not in out:
            out.append(str(found))
    if not out:
        raise SystemExit("no C++ compiler found; set CXX or pass --cxx")
    return out


def check_atomic_load_alignment(root: Path) -> None:
    source = root / "src" / "uwvm2" / "runtime" / "compiler" / "llvm_jit" / "compile_all_from_uwvm" / "translate" / "single_func_emit.h"
    lines = source.read_text(encoding="utf-8").splitlines()
    for i, line in enumerate(lines):
        if "setAtomic(::llvm::AtomicOrdering::Acquire)" not in line:
            continue
        window = "\n".join(lines[max(0, i - 8) : i + 1])
        if "setAlignment(::llvm::Align{alignof(::std::uintptr_t)})" not in window:
            raise SystemExit(f"pointer-sized acquire load near {source}:{i + 1} is missing explicit uintptr_t alignment")


def compile_or_run(cmd: list[str]) -> None:
    print("INFO:", quote_cmd(cmd))
    subprocess.run(cmd, check=True)


def pick_clang(explicit: str | None) -> str:
    candidates: list[str | None] = [
        explicit,
        os.environ.get("CLANGXX"),
        os.environ.get("CXX"),
        r"D:\tool-chain\x86_64-windows-gnu\llvm\bin\clang++.exe",
        "clang++",
    ]
    for cxx in candidates:
        if not cxx:
            continue
        found = shutil.which(cxx) if not Path(cxx).exists() else cxx
        if found:
            return str(found)
    raise SystemExit("no clang++ found for Windows target matrix; set CLANGXX or pass --matrix-cxx")


def windows_msvc_compile_matrix(script_dir: Path, root: Path, cxx: str, sysroot: Path) -> None:
    include = sysroot / "include"
    msvc_stl = include / "c++" / "msstl"
    if not include.exists():
        raise SystemExit(f"MSVC sysroot include directory not found: {include}")

    out_dir = root / "build" / "test" / "0015.backend_fuzzer" / "win32_abi_probe_matrix"
    out_dir.mkdir(parents=True, exist_ok=True)

    targets = [
        ("x86_64-windows-msvc", "x86_64-unknown-windows-msvc"),
        ("i686-windows-msvc", "i686-unknown-windows-msvc"),
        ("aarch64-windows-msvc", "aarch64-unknown-windows-msvc"),
        ("aarch64ec-windows-msvc", "arm64ec-unknown-windows-msvc"),
        ("arm-windows-msvc", "armv7-unknown-windows-msvc"),
    ]

    common = [
        cxx,
        "-std=c++20",
        "-O2",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-fms-compatibility",
        "-isystem",
        str(include),
    ]
    if msvc_stl.exists():
        common.extend(["-isystem", str(msvc_stl)])

    for label, target in targets:
        out = out_dir / f"{label}.obj"
        cmd = [*common]
        if label == "arm-windows-msvc":
            # Current MSVC sysroots deliberately reject 32-bit ARM CRT/STL headers.
            # Keep the ABI probe meaningful by compiling the same call-boundary code
            # with Clang builtin scalar types and no Windows headers for this target.
            cmd = [
                cxx,
                "-std=c++20",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-fms-compatibility",
                "-DUWVM_TEST_FREESTANDING_C_TYPES",
            ]
        cmd.extend([f"--target={target}", "-c", str(script_dir / "win32_abi_probe.cc"), "-o", str(out)])
        compile_or_run(cmd)
        print(f"INFO: compile-only probe passed for {label}")


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    root = script_dir.parents[1]

    ap = argparse.ArgumentParser()
    ap.add_argument("--cxx", default=None)
    ap.add_argument("--matrix-cxx", default=None)
    ap.add_argument("--msvc-sysroot", type=Path, default=Path(r"D:\tool-chain\windows-msvc-sysroot"))
    ap.add_argument("--out", type=Path, default=root / "build" / "test" / "0015.backend_fuzzer" / "win32_abi_probe.exe")
    ap.add_argument("--no-source-check", action="store_true")
    ap.add_argument("--windows-target-matrix", action="store_true")
    ap.add_argument("extra", nargs=argparse.REMAINDER)
    args = ap.parse_args()

    if not args.no_source_check:
        check_atomic_load_alignment(root)

    out = args.out.resolve()
    out.parent.mkdir(parents=True, exist_ok=True)

    last_error: subprocess.CalledProcessError | None = None
    for cxx in cxx_candidates(args.cxx):
        std = pick_std(cxx)
        cmd = [
            cxx,
            std,
            "-O2",
            "-g",
            "-Wall",
            "-Wextra",
            "-Werror",
            str(script_dir / "win32_abi_probe.cc"),
            "-o",
            str(out),
            *args.extra,
        ]
        try:
            compile_or_run(cmd)
            break
        except subprocess.CalledProcessError as e:
            last_error = e
            if args.cxx:
                raise
            print(f"WARN: compiler failed, trying next candidate: {cxx}", file=sys.stderr)
    else:
        assert last_error is not None
        raise last_error

    compile_or_run([str(out)])
    print("INFO: win32 ABI probe passed")
    if args.windows_target_matrix:
        windows_msvc_compile_matrix(script_dir, root, pick_clang(args.matrix_cxx), args.msvc_sysroot)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

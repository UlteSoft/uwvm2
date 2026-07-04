#!/usr/bin/env python3
"""Build the backend libFuzzer target without xmake."""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import time
from pathlib import Path

from build_runner import (
    add_matrix_defines,
    add_memory_model_defines,
    filtered_llvm_cxxflags,
    llvm_config,
    llvm_lib_flags,
    pick_std_flag,
    run_text,
    split_env_words,
)


def env_bool(name: str, default: bool = False) -> bool:
    value = os.environ.get(name)
    if value is None:
        return default
    return value not in {"", "0", "false", "False", "no", "NO", "off", "OFF"}


def env_float(name: str, default: float) -> float:
    value = os.environ.get(name)
    if not value:
        return default
    try:
        return float(value)
    except ValueError:
        return default


def run_with_progress(cmd: list[str]) -> int:
    progress = env_bool("UWVM_BACKEND_LIBFUZZER_PROGRESS", True)
    interval = max(env_float("UWVM_BACKEND_LIBFUZZER_PROGRESS_INTERVAL", 10.0), 1.0)
    if not progress:
        return subprocess.run(cmd).returncode

    print(
        "INFO: backend libFuzzer build command started "
        "(set UWVM_BACKEND_LIBFUZZER_PROGRESS=0 to silence heartbeat)",
        flush=True,
    )
    start = time.monotonic()
    proc = subprocess.Popen(cmd)
    while proc.poll() is None:
        time.sleep(interval)
        if proc.poll() is None:
            elapsed = int(time.monotonic() - start)
            print(f"INFO: still building backend libFuzzer target ({elapsed}s elapsed)", flush=True)
    return proc.returncode


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    root = script_dir.parents[1]

    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--combine", default=os.environ.get("UWVM_BACKEND_FUZZER_COMBINE", "none"))
    ap.add_argument("--delay", default=os.environ.get("UWVM_BACKEND_FUZZER_DELAY", "none"))
    ap.add_argument("--memory-model", default=os.environ.get("UWVM_BACKEND_FUZZER_MEMORY_MODEL", "default"))
    ap.add_argument("--use-thread-local", action="store_true", default=os.environ.get("UWVM_BACKEND_FUZZER_USE_THREAD_LOCAL", "0") not in {"", "0", "false", "no"})
    ap.add_argument("--define", action="append", default=[])
    ap.add_argument("--extra-cxxflag", action="append", default=[])
    ap.add_argument("--extra-ldflag", action="append", default=[])
    ap.add_argument("--sanitizers", default=os.environ.get("UWVM_BACKEND_LIBFUZZER_SANITIZERS", "fuzzer,address,undefined"))
    ap.add_argument("--opt", default=os.environ.get("UWVM_BACKEND_LIBFUZZER_OPT", "-O1"))
    ap.add_argument("--sysroot", default=os.environ.get("UWVM_BACKEND_LIBFUZZER_SYSROOT", os.environ.get("SYSROOT", os.environ.get("SDKROOT", ""))))
    ap.add_argument("--use-lld", action=argparse.BooleanOptionalAction, default=None)
    args = ap.parse_args()
    if args.use_lld is None:
        args.use_lld = env_bool("UWVM_BACKEND_LIBFUZZER_USE_LLD", bool(args.sysroot))

    cxx = os.environ.get("CXX", "c++")
    std_flag = pick_std_flag(cxx)
    llvm = llvm_config()

    out = args.out.resolve()
    out.parent.mkdir(parents=True, exist_ok=True)

    include_dirs = [
        root / "src",
        root / "third-parties" / "fast_float" / "include",
        root / "third-parties" / "fast_io" / "include",
        root / "third-parties" / "bizwen" / "include",
        root / "third-parties" / "boost_unordered" / "include",
    ]

    defines = [
        "-DUWVM=2",
        "-DUWVM_TEST=2",
        "-DUWVM_VERSION_X=2",
        "-DUWVM_VERSION_Y=0",
        "-DUWVM_VERSION_Z=3",
        "-DUWVM_VERSION_S=0",
        "-DUWVM_USE_UWVM_INT",
        "-DUWVM_USE_LLVM_JIT",
        "-DUWVM_DISABLE_DEBUG_INT",
        "-DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519",
    ]
    add_matrix_defines(defines, args.combine, args.delay)
    add_memory_model_defines(defines, args.memory_model)
    if args.use_thread_local:
        defines.append("-DUWVM_USE_THREAD_LOCAL")
    for define in [*args.define, *split_env_words("UWVM_BACKEND_FUZZER_EXTRA_DEFINES")]:
        defines.append(define if define.startswith("-D") else f"-D{define}")

    sanitizer_flag = f"-fsanitize={args.sanitizers}" if args.sanitizers else ""
    sysroot_flags = [f"--sysroot={args.sysroot}"] if args.sysroot else []
    linker_driver_flags = ["-fuse-ld=lld"] if args.use_lld else []
    llvm_ldflags = shlex.split(run_text([llvm, "--ldflags"]))
    llvm_system_libs = shlex.split(run_text([llvm, "--system-libs"], check=False))
    llvm_libs = llvm_lib_flags(llvm)
    llvm_libdir = run_text([llvm, "--libdir"], check=False)

    cmd = [
        cxx,
        std_flag,
        args.opt,
        "-g",
        "-fno-omit-frame-pointer",
        "-pthread",
        *([sanitizer_flag] if sanitizer_flag else []),
        *sysroot_flags,
        *linker_driver_flags,
        *defines,
        *(f"-I{p}" for p in include_dirs),
        *filtered_llvm_cxxflags(llvm),
        *split_env_words("UWVM_BACKEND_FUZZER_EXTRA_CXXFLAGS"),
        *args.extra_cxxflag,
        str(script_dir / "backend_libfuzzer.cc"),
        str(root / "src" / "uwvm2" / "runtime" / "lib" / "uwvm_runtime.default.cpp"),
        "-o",
        str(out),
        *llvm_ldflags,
        *llvm_libs,
        *llvm_system_libs,
        "-lssl",
        "-lcrypto",
        *([sanitizer_flag] if sanitizer_flag else []),
        *split_env_words("UWVM_BACKEND_FUZZER_EXTRA_LDFLAGS"),
        *args.extra_ldflag,
    ]

    if llvm_libdir:
        cmd.append(f"-Wl,-rpath,{llvm_libdir}")

    print("INFO: building backend libFuzzer target")
    print("INFO: combine=", args.combine, " delay=", args.delay, " sanitizers=", args.sanitizers, sep="")
    if args.sysroot:
        print("INFO: sysroot=", args.sysroot, sep="")
    print("INFO: use-lld=", int(args.use_lld), sep="")
    rc = run_with_progress(cmd)
    if rc != 0:
        return rc
    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

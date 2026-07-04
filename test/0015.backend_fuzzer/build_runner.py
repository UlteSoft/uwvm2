#!/usr/bin/env python3
"""Build the backend-fuzzer direct-storage runner without xmake."""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path


REQUIRED_LLVM_COMPONENTS = [
    "core",
    "support",
    "analysis",
    "target",
    "linker",
    "executionengine",
    "mcjit",
    "runtimedyld",
    "passes",
    "scalaropts",
    "transformutils",
    "instcombine",
    "bitreader",
    "bitwriter",
    "object",
    "debuginfodwarf",
]

OPTIONAL_LLVM_COMPONENTS = ["targetparser"]


def run_text(argv: list[str], *, check: bool = True) -> str:
    proc = subprocess.run(argv, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check and proc.returncode != 0:
        raise SystemExit(f"command failed: {' '.join(argv)}\n{proc.stderr}")
    return proc.stdout.strip()


def compiler_accepts(cxx: str, flag: str) -> bool:
    with tempfile.NamedTemporaryFile("w", suffix=".cc", delete=False) as src:
        src.write("int main() { return 0; }\n")
        src_path = src.name
    try:
        proc = subprocess.run(
            [cxx, flag, "-x", "c++", "-fsyntax-only", src_path],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return proc.returncode == 0
    finally:
        try:
            os.unlink(src_path)
        except OSError:
            pass


def pick_std_flag(cxx: str) -> str:
    env = os.environ.get("UWVM_BACKEND_FUZZER_CXXSTD")
    if env:
        return env if env.startswith("-std=") else f"-std={env}"
    for flag in ["-std=c++26", "-std=c++2c", "-std=c++23"]:
        if compiler_accepts(cxx, flag):
            return flag
    raise SystemExit(f"{cxx} does not accept any of -std=c++26, -std=c++2c, -std=c++23")


def llvm_config() -> str:
    tool = os.environ.get("LLVM_CONFIG", "llvm-config")
    try:
        run_text([tool, "--version"])
    except FileNotFoundError:
        raise SystemExit("llvm-config not found; set LLVM_CONFIG or put llvm-config on PATH") from None
    return tool


def llvm_components(tool: str) -> set[str]:
    try:
        return set(run_text([tool, "--components"]).split())
    except SystemExit:
        return set()


def llvm_lib_flags(tool: str) -> list[str]:
    available = llvm_components(tool)
    components = REQUIRED_LLVM_COMPONENTS[:]
    components += [c for c in OPTIONAL_LLVM_COMPONENTS if c in available]
    if "nativecodegen" in available:
        components.append("nativecodegen")
    elif "native" in available:
        components.append("native")
    else:
        components.append("all-targets")

    candidates = [["all"], components] if sys.platform == "win32" else [components, ["all"]]
    for candidate in candidates:
        proc = subprocess.run([tool, "--libs", *candidate], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if proc.returncode == 0 and proc.stdout.strip():
            return shlex.split(proc.stdout)
    raise SystemExit("failed to query LLVM libraries from llvm-config")


def filtered_llvm_cxxflags(tool: str) -> list[str]:
    flags = shlex.split(run_text([tool, "--cxxflags"]))
    out: list[str] = []
    skip_next = False
    for flag in flags:
        if skip_next:
            skip_next = False
            continue
        if flag.startswith("-std=") or flag == "-fno-exceptions":
            continue
        if flag in {"-I", "-isystem"}:
            out.append(flag)
            skip_next = False
            continue
        out.append(flag)
    return out


def has_library(directory: Path, name: str) -> bool:
    for filename in (f"lib{name}.dll.a", f"lib{name}.a", f"{name}.lib", f"lib{name}.lib"):
        if (directory / filename).is_file():
            return True
    return False


def libcxx_runtime_flags(tool: str, cxxflags: list[str]) -> tuple[list[str], list[str], list[str]]:
    if "-stdlib=libc++" not in cxxflags:
        return [], [], []

    prefix = Path(run_text([tool, "--prefix"], check=False) or "")
    libdir = Path(run_text([tool, "--libdir"], check=False) or "")
    host_target = run_text([tool, "--host-target"], check=False)

    roots: list[Path] = []
    for root in (prefix, prefix.parent if str(prefix) else Path(), prefix / "runtimes", prefix.parent / "runtimes" if str(prefix) else Path(),
                 libdir.parent if str(libdir) else Path(), libdir.parent / "runtimes" if str(libdir) else Path()):
        if str(root) and root not in roots:
            roots.append(root)

    targets: list[str] = []
    for target in (
        host_target,
        host_target.replace("-unknown-", "-") if host_target else "",
        host_target.replace("-unknown-", "-w64-") if host_target else "",
    ):
        if target and target not in targets:
            targets.append(target)

    include_dir: Path | None = None
    link_dirs: list[Path] = []
    for root in roots:
        include_candidates = [root / "include" / "c++" / "v1", root / "runtimes" / "include" / "c++" / "v1"]
        include_candidates += [root / target / "include" / "c++" / "v1" for target in targets]
        for candidate in include_candidates:
            if include_dir is None and (candidate / "algorithm").is_file():
                include_dir = candidate

        link_candidates = [root / "lib", root / "runtimes" / "lib"]
        for target in targets:
            link_candidates += [root / "lib" / target, root / "runtimes" / "lib" / target, root / target / "lib"]
        for candidate in link_candidates:
            if has_library(candidate, "c++") and candidate not in link_dirs:
                link_dirs.append(candidate)

    extra_cxxflags = ["-isystem", str(include_dir)] if include_dir is not None else []
    extra_ldflags: list[str] = []
    extra_libs: list[str] = []
    if link_dirs:
        extra_ldflags.append("-nostdlib++")
        extra_ldflags += [f"-L{directory}" for directory in link_dirs]
        extra_libs.append("-lc++")
        if any(has_library(directory, "c++abi") for directory in link_dirs):
            extra_libs.append("-lc++abi")
        if any(has_library(directory, "unwind") for directory in link_dirs):
            extra_libs.append("-lunwind")
    return extra_cxxflags, extra_ldflags, extra_libs


def llvm_toolchain_flags(tool: str) -> tuple[list[str], list[str]]:
    if sys.platform != "win32":
        return [], []

    host_target = run_text([tool, "--host-target"], check=False)
    if not host_target:
        return [], []

    target = host_target.replace("-unknown-", "-w64-")
    prefix_text = run_text([tool, "--prefix"], check=False)
    linkflags = ["-fuse-ld=lld"]
    if prefix_text:
        target_libdir = Path(prefix_text).parent / target.replace("-w64-", "-") / "lib"
        if target_libdir.is_dir():
            linkflags.append(f"-L{target_libdir}")
    return [f"--target={target}"], linkflags


def add_matrix_defines(defines: list[str], combine: str, delay: str) -> None:
    if combine not in {"none", "soft", "heavy", "extra"}:
        raise SystemExit(f"unsupported --combine: {combine}")
    if delay not in {"none", "soft", "heavy"}:
        raise SystemExit(f"unsupported --delay: {delay}")

    if combine != "none":
        defines.append("-DUWVM_ENABLE_UWVM_INT_COMBINE_OPS")
        if combine in {"heavy", "extra"}:
            defines.append("-DUWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS")
        if combine == "extra":
            defines.append("-DUWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS")

    if delay in {"soft", "heavy"}:
        defines.append("-DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT")
        if delay == "heavy":
            defines.append("-DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY")


def add_memory_model_defines(defines: list[str], memory_model: str) -> None:
    if memory_model == "default":
        return
    if memory_model == "mmap":
        defines.append("-DUWVM_FORCE_USE_MMAP")
        return
    if memory_model == "single-thread-alloc":
        defines.append("-DUWVM_FORCE_DISABLE_MMAP")
        return
    if memory_model == "multi-thread-alloc":
        defines.append("-DUWVM_FORCE_DISABLE_MMAP")
        defines.append("-DUWVM_USE_MULTITHREAD_ALLOCATOR")
        return
    raise SystemExit(f"unsupported --memory-model: {memory_model}")


def split_env_words(name: str) -> list[str]:
    value = os.environ.get(name, "")
    return shlex.split(value.replace(",", " ")) if value else []


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    root = script_dir.parents[1]

    ap = argparse.ArgumentParser()
    ap.add_argument("--generated-dir", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--combine", default=os.environ.get("UWVM_BACKEND_FUZZER_COMBINE", "none"))
    ap.add_argument("--delay", default=os.environ.get("UWVM_BACKEND_FUZZER_DELAY", "none"))
    ap.add_argument("--memory-model", default=os.environ.get("UWVM_BACKEND_FUZZER_MEMORY_MODEL", "default"))
    ap.add_argument("--use-thread-local", action="store_true", default=os.environ.get("UWVM_BACKEND_FUZZER_USE_THREAD_LOCAL", "0") not in {"", "0", "false", "no"})
    ap.add_argument("--define", action="append", default=[])
    ap.add_argument("--extra-cxxflag", action="append", default=[])
    ap.add_argument("--extra-ldflag", action="append", default=[])
    args = ap.parse_args()

    cxx = os.environ.get("CXX", "c++")
    llvm = llvm_config()
    toolchain_cxxflags, toolchain_ldflags = llvm_toolchain_flags(llvm)
    std_flag = pick_std_flag(cxx)
    llvm_cxxflags = filtered_llvm_cxxflags(llvm)
    libcxx_cxxflags, libcxx_ldflags, libcxx_libs = libcxx_runtime_flags(llvm, llvm_cxxflags)

    generated_dir = args.generated_dir.resolve()
    out = args.out.resolve()
    out.parent.mkdir(parents=True, exist_ok=True)

    include_dirs = [
        generated_dir,
        generated_dir / "generated",
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
        "-DUWVM_VERSION_Z=2",
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

    llvm_ldflags = shlex.split(run_text([llvm, "--ldflags"]))
    llvm_system_libs = shlex.split(run_text([llvm, "--system-libs"], check=False))
    if sys.platform == "win32" and "-lws2_32" not in llvm_system_libs:
        llvm_system_libs.append("-lws2_32")
    llvm_libs = llvm_lib_flags(llvm)
    llvm_libdir = run_text([llvm, "--libdir"], check=False)

    cmd = [
        cxx,
        *toolchain_cxxflags,
        std_flag,
        "-O2",
        "-g",
        *defines,
        *(f"-I{p}" for p in include_dirs),
        *llvm_cxxflags,
        *libcxx_cxxflags,
        *split_env_words("UWVM_BACKEND_FUZZER_EXTRA_CXXFLAGS"),
        *args.extra_cxxflag,
        str(script_dir / "backend_fuzzer_runner.cc"),
        str(root / "src" / "uwvm2" / "runtime" / "lib" / "uwvm_runtime.default.cpp"),
        "-o",
        str(out),
        *toolchain_ldflags,
        *libcxx_ldflags,
        *llvm_ldflags,
        *llvm_libs,
        *libcxx_libs,
        *llvm_system_libs,
        "-lssl",
        "-lcrypto",
        *split_env_words("UWVM_BACKEND_FUZZER_EXTRA_LDFLAGS"),
        *args.extra_ldflag,
    ]

    if sys.platform != "win32":
        cmd.insert(4, "-pthread")

    if llvm_libdir:
        cmd.append(f"-Wl,-rpath,{llvm_libdir}")

    print("INFO: building backend fuzzer runner")
    print("INFO: combine=", args.combine, " delay=", args.delay, sep="")
    proc = subprocess.run(cmd)
    if proc.returncode != 0:
        return proc.returncode
    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Exercise production NaN lowering on MIPS O32/N32/N64, BE/LE and optional SPARC/m68k.

MIPS tests require LLVM tools and qemu-user, not a libc/C++ SDK. The optional
--extra-sdk-root supplies GCC 15 cross toolchains/sysroots for SPARC64 and m68k.
Tests arithmetic, sqrt, width conversion, vectors and unchanged sign/transport.
These are generated-object tests, not full native LLVM JIT installations.
"""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--cxx", default="clang++")
parser.add_argument("--cc", default="clang")
parser.add_argument("--llvm-config", default="llvm-config")
parser.add_argument("--llc", default="llc")
parser.add_argument("--opt", default="opt")
parser.add_argument("--qemu-dir", type=Path, default=Path("/usr/bin"))
parser.add_argument("--build-dir", type=Path)
parser.add_argument("--extra-sdk-root", type=Path)
parser.add_argument("--only", help="Comma-separated subset of target names")
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
fixtures = Path(__file__).resolve().parent / "fixtures"
build = (args.build_dir or Path(tempfile.mkdtemp(prefix="uwvm-native-nan-"))).resolve()
build.mkdir(parents=True, exist_ok=True)
mips_targets = [
    ("mips", "mips-linux-gnu", "24Kf", "mips32r2", "legacy"),
    ("mipsel", "mipsel-linux-gnu", "24Kf", "mips32r2", "legacy"),
    ("mips64", "mips64-linux-gnuabi64", "MIPS64R2-generic", "mips64r2", "legacy"),
    ("mips64el", "mips64el-linux-gnuabi64", "MIPS64R2-generic", "mips64r2", "legacy"),
    ("mipsn32", "mips64-linux-gnuabin32", "MIPS64R2-generic", "mips64r2", "legacy"),
    ("mipsn32el", "mips64el-linux-gnuabin32", "MIPS64R2-generic", "mips64r2", "legacy"),
    ("mips-nan2008", "mips-linux-gnu", "mips32r6-generic", "mips32r6", "2008"),
    ("mipsel-nan2008", "mipsel-linux-gnu", "mips32r6-generic", "mips32r6", "2008"),
    ("mips64-nan2008", "mips64-linux-gnuabi64", "I6400", "mips64r6", "2008"),
    ("mips64el-nan2008", "mips64el-linux-gnuabi64", "I6400", "mips64r6", "2008"),
    ("mipsn32-nan2008", "mips64-linux-gnuabin32", "I6400", "mips64r6", "2008"),
    ("mipsn32el-nan2008", "mips64el-linux-gnuabin32", "I6400", "mips64r6", "2008"),
]
extra_targets = [("sparc64", "sparc64-linux-gnu", "v9"), ("m68k", "m68k-linux-gnu", "M68040")]
available = {target[0] for target in mips_targets + (extra_targets if args.extra_sdk_root else [])}
selected = set(args.only.split(",")) if args.only else available
if not selected <= available:
    parser.error("Unavailable targets (SPARC/m68k need --extra-sdk-root): " + ", ".join(sorted(selected - available)))


def run(command, label, required=True):
    result = subprocess.run([str(x) for x in command], capture_output=True, text=True, timeout=300)
    (build / (label + ".log")).write_text(result.stdout + result.stderr)
    if required and result.returncode:
        print(result.stdout + result.stderr, flush=True)
        result.check_returncode()
    return result


def config(*options):
    return shlex.split(subprocess.check_output([args.llvm_config, *options], text=True))


def optimized_ir(name, mode, original, fixed):
    if mode == "before":
        return original
    if mode == "none":
        return fixed
    ir = build / (name + "-" + mode + ".ll")
    run([args.opt, "-S", "-passes=default<" + mode + ">", "-verify-each", fixed, "-o", ir], name + "-" + mode + "-opt")
    return ir


def report(name, mode, result):
    if mode == "before":
        if result.returncode != 1:
            raise RuntimeError("Expected a semantic failure before lowering: " + name)
        print(name + ": reproduced baseline failure", flush=True)
    else:
        print(name + "-" + mode + ": PASS", flush=True)


lower = build / "lower"
run([args.cxx, *config("--cxxflags"), "-std=c++20", "-O2", "-I" + str(repo / "src"),
     fixtures / "fp_legacy_nan_lowering.cpp", *config("--ldflags", "--libs", "--system-libs"), "-o", lower], "host-build")
for name, triple, cpu, arch, nan_encoding in mips_targets:
    if name not in selected:
        continue
    # ABI and NaN encoding are independent test dimensions. A canonicalizing
    # legacy test cannot certify the NaN2008 native fast path. Use R6 CPUs for
    # NaN2008 so ELF encoding and QEMU FPU mode agree without a target libc.
    flags = ["--target=" + triple, "-march=" + arch, "-mnan=" + nan_encoding, "-O2", "-ffp-contract=off", "-fno-math-errno", "-fno-trapping-math"]
    original, fixed = build / (name + "-original.ll"), build / (name + "-fixed.ll")
    run([args.cc, *flags, "-S", "-emit-llvm", fixtures / "fp_legacy_nan_input.c", "-o", original], name + "-input")
    run([lower, original, fixed, *(["modern-nan"] if nan_encoding == "2008" else [])], name + "-lower")
    for mode in ["before", "none", "O2", "O3"]:
        ir = optimized_ir(name, mode, original, fixed)
        obj, binary = build / (name + "-" + mode + ".o"), build / (name + "-" + mode)
        # Function target attributes alone do not set all ELF ISA/NaN flags.
        # Match llc's module target to the native runner, especially R6/NaN2008;
        # otherwise an ISA-mismatched object may fail before the FP test executes.
        run([args.llc, "-O3", "-mcpu=" + arch, "-mattr=" + ("+nan2008" if nan_encoding == "2008" else "-nan2008"),
             "-filetype=obj", ir, "-o", obj], name + "-" + mode + "-codegen")
        run([args.cc, *flags, "-fno-builtin", "-fno-stack-protector", "-nostdlib", "-static", "-fuse-ld=lld", "-Wl,-e,bare_entry",
             fixtures / "fp_legacy_nan_runner.c", obj, "-o", binary], name + "-" + mode + "-link")
        expected_failure = mode == "before" and nan_encoding == "legacy"
        result = run([args.qemu_dir / ("qemu-" + name.split("-")[0]), "-cpu", cpu, binary], name + "-" + mode + "-run", not expected_failure)
        if expected_failure:
            report(name, mode, result)
        else:
            print(name + "-" + mode + ": PASS", flush=True)

if args.extra_sdk_root:
    sdk = args.extra_sdk_root.resolve()
    for name, triple, cpu in extra_targets:
        if name not in selected:
            continue
        original, fixed = build / (name + "-original.ll"), build / (name + "-fixed.ll")
        run([args.cc, "--target=" + triple, "-O2", "-fno-math-errno", "-ffp-contract=off", "-S", "-emit-llvm",
             fixtures / "fp_legacy_nan_input.c", "-o", original], name + "-input")
        run([lower, original, fixed, *(["extended"] if name == "m68k" else [])], name + "-lower")
        bridge = []
        if name == "m68k":
            bridge = [build / "m68k-bridge.o"]
            run([sdk / ("usr/bin/" + triple + "-g++-15"), "--sysroot=" + str(sdk), "-std=c++20", "-O3",
                 "-I" + str(repo / "src"), "-c", fixtures / "fp_legacy_nan_bridge.cpp", "-o", bridge[0]], name + "-bridge")
        for mode in ["before", "none", "O2", "O3"]:
            ir = optimized_ir(name, mode, original, fixed)
            obj, binary = build / (name + "-" + mode + ".o"), build / (name + "-" + mode)
            run([args.llc, "-O3", "-mcpu=" + cpu, "-filetype=obj", ir, "-o", obj], name + "-" + mode + "-codegen")
            run([sdk / ("usr/bin/" + triple + "-gcc-15"), "--sysroot=" + str(sdk), "-O2", "-fno-builtin", "-no-pie",
                 fixtures / "fp_legacy_nan_runner.c", obj, *bridge, "-lm", "-o", binary], name + "-" + mode + "-link")
            result = run([args.qemu_dir / ("qemu-" + name), "-L", sdk / ("usr/" + triple), binary], name + "-" + mode + "-run", mode != "before")
            report(name, mode, result)
print("Artifacts:", build)

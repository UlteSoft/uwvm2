#!/usr/bin/env python3
"""Check native LLVM rounding, including RISC-V sNaN preservation regressions.

Uses GCC 15 sysroots under --sdk-root and optionally --extra-sdk-root. The native
x86_64 runner uses --cxx. Generated objects are executed with qemu-user for cross
targets. Retains IR and logs for baseline and none/O2/O3 production lowering.
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
parser.add_argument("--sdk-root", type=Path, required=True)
parser.add_argument("--extra-sdk-root", type=Path)
parser.add_argument("--qemu-dir", type=Path, default=Path("/usr/bin"))
parser.add_argument("--build-dir", type=Path)
parser.add_argument("--only", help="Comma-separated target names")
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
fixtures = Path(__file__).resolve().parent / "fixtures"
build = (args.build_dir or Path(tempfile.mkdtemp(prefix="uwvm-native-rounding-"))).resolve()
build.mkdir(parents=True, exist_ok=True)
targets = [
    ("x86_64", "x86_64-linux-gnu", None),
    ("aarch64", "aarch64-linux-gnu", "aarch64"),
    ("armhf", "arm-linux-gnueabihf", "arm"),
    ("ppc32", "powerpc-linux-gnu", "ppc"),
    ("ppc64be", "powerpc64-linux-gnu", "ppc64"),
    ("ppc64le", "powerpc64le-linux-gnu", "ppc64le"),
    ("riscv64", "riscv64-linux-gnu", "riscv64"),
    ("s390x", "s390x-linux-gnu", "s390x"),
    ("loongarch64", "loongarch64-linux-gnu", "loongarch64"),
]
selected = set(args.only.split(",")) if args.only else {t[0] for t in targets}
if not selected <= {t[0] for t in targets}:
    parser.error("Unknown target in --only")


def run(command, label, required=True):
    result = subprocess.run([str(x) for x in command], capture_output=True, text=True, timeout=300)
    (build / (label + ".log")).write_text(result.stdout + result.stderr)
    if required and result.returncode:
        print(result.stdout + result.stderr, flush=True)
        result.check_returncode()
    return result


def config(*options):
    return shlex.split(subprocess.check_output([args.llvm_config, *options], text=True))


lower = build / "lower"
run([args.cxx, *config("--cxxflags"), "-std=c++20", "-O2", "-I" + str(repo / "src"),
     fixtures / "fp_legacy_nan_lowering.cpp", *config("--ldflags", "--libs", "--system-libs"), "-o", lower], "host-build")
for name, triple, emulator in targets:
    if name not in selected:
        continue
    sdk = args.sdk_root.resolve()
    if emulator and not (sdk / ("usr/bin/" + triple + "-g++-15")).exists() and args.extra_sdk_root:
        sdk = args.extra_sdk_root.resolve()
    compiler = [sdk / ("usr/bin/" + triple + "-g++-15"), "--sysroot=" + str(sdk)] if emulator else [args.cxx]
    original, fixed = build / (name + "-original.ll"), build / (name + "-fixed.ll")
    run([args.cc, "--target=" + triple, "-O2", "-fno-math-errno", "-fno-trapping-math", "-ffp-contract=off",
         "-S", "-emit-llvm", fixtures / "fp_native_rounding_input.c", "-o", original], name + "-input")
    prepared = build / (name + "-prepared.ll")
    run([lower, original, prepared, "native-before"], name + "-prepare")
    original = prepared
    run([lower, original, fixed, "native"], name + "-lower")
    constrained = build / (name + "-constrained.ll")
    run([lower, original, constrained, "native-constrained"], name + "-constrain")
    for mode in ["before", "none", "O2", "O3", "constrained-none", "constrained-O2", "constrained-O3"]:
        source = constrained if mode.startswith("constrained-") else fixed
        ir = original if mode == "before" else source
        pipeline = mode.split("-")[-1]
        if pipeline in ("O2", "O3"):
            ir = build / (name + "-" + mode + ".ll")
            run([args.opt, "-S", "-passes=default<" + pipeline + ">", "-verify-each", source, "-o", ir], name + "-" + mode + "-opt")
        obj, binary = build / (name + "-" + mode + ".o"), build / (name + "-" + mode)
        run([args.llc, "-O3", "-filetype=obj", ir, "-o", obj], name + "-" + mode + "-codegen")
        run([*compiler, "-std=c++20", "-O2", "-no-pie", "-I" + str(repo / "src"),
             fixtures / "fp_native_rounding_runner.cpp", obj, "-lm", "-o", binary], name + "-" + mode + "-link")
        command = [args.qemu_dir / ("qemu-" + emulator), "-L", sdk / ("usr/" + triple), binary] if emulator else [binary]
        expected_failure = mode == "before" and name == "riscv64"
        result = run(command, name + "-" + mode + "-run", not expected_failure)
        if expected_failure and result.returncode != 1:
            raise RuntimeError("Expected the baseline RISC-V semantic failure")
        print(name + "-" + mode + (": baseline failure reproduced" if expected_failure else ": PASS"), flush=True)
print("Artifacts:", build)

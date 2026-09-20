#!/usr/bin/env python3
"""Build LLVM FP-bit regression objects and execute them using an i386 SDK.

Run from any directory. Set --run-prefix to a native i686 loader when the host
does not install /lib/ld-linux.so.2. This intentionally tests generated objects,
not a native i386 LLVM JIT installation. Fixtures are .cpp so xmake does not
register the cross-linked runner as a standalone unit test.
"""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--cxx", default="clang++")
parser.add_argument("--llvm-config", default="llvm-config")
parser.add_argument("--llc", default="llc")
parser.add_argument("--opt", default="opt")
parser.add_argument("--i686-cxx", default="i686-linux-gnu-g++")
parser.add_argument("--i686-flags", default="")
parser.add_argument("--run-prefix", default="")
parser.add_argument("--objdump", default="objdump")
parser.add_argument("--build-dir", type=Path)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
fixtures = Path(__file__).resolve().parent / "fixtures"
build = args.build_dir or Path(tempfile.mkdtemp(prefix="uwvm-llvm-fp-bits-"))
build = build.resolve()
build.mkdir(parents=True, exist_ok=True)


def run(command, label):
    result = subprocess.run([str(x) for x in command], cwd=build,
                            text=True, capture_output=True, timeout=300)
    (build / (label + ".log")).write_text(result.stdout + result.stderr)
    if result.returncode:
        print(result.stdout + result.stderr, flush=True)
        result.check_returncode()
    return result.stdout


def config(*options):
    return shlex.split(subprocess.check_output([args.llvm_config, *options], text=True))


lower = build / "lower"
run([args.cxx, *config("--cxxflags"), "-std=c++20", "-O2",
     "-I" + str(repo / "src"), fixtures / "fp_bits_lowering.cpp",
     *config("--ldflags", "--libs", "--system-libs"), "-o", lower], "host-build")
for label, features, cpu in [
    ("nosse", "-sse,-sse2", "pentiumpro"),
    ("sse2", "+sse2", "pentium4"),
    ("sse41", "+sse2,+sse4.1", "penryn"),
]:
    original = build / (label + ".ll")
    run([lower, features, original], label + "-lower")
    for pipeline in ["none", "O2", "O3"]:
        name = label + "-" + pipeline
        ir = original
        if pipeline != "none":
            ir = build / (name + ".ll")
            run([args.opt, "-S", "-passes=default<" + pipeline + ">", "-verify-each",
                 original, "-o", ir], name + "-optimize")
        obj = build / (name + ".o")
        run([args.llc, "-O3", "-mtriple=i386-linux-gnu", "-mcpu=" + cpu,
             "-filetype=obj", ir, "-o", obj], name + "-codegen")
        # A bridge compiled for SSE2 can behave differently from an x87 bridge
        # even when both execute the same generated object.
        for host, flags in [("x87", []), ("sse2", ["-msse2", "-mfpmath=sse"])]:
            variant = name + "-bridge-" + host
            runner = build / (variant + "-runner")
            run([args.i686_cxx, *shlex.split(args.i686_flags), "-std=c++20", "-O3",
                 "-fno-math-errno", "-fno-trapping-math", "-fno-rounding-math", "-ffp-contract=off", *flags,
                 "-I" + str(repo / "src"), "-fno-pie", "-no-pie",
                 fixtures / "fp_bits_runner.cpp", obj, "-lm", "-o", runner], variant + "-link")
            run([*shlex.split(args.run_prefix), runner], variant + "-run")
            print(variant + ": PASS", flush=True)
        assembly = run([args.objdump, "-dr", obj], name + "-assembly")
        (build / (name + ".asm")).write_text(assembly)
print("Artifacts:", build)

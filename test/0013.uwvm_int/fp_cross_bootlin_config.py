#!/usr/bin/env python3
"""Generate reproducible FP configs for the Bootlin 2026.08 GCC 15.3 SDKs.

Install the exact SDK directories listed below yourself (verify their published
SHA256 sums). This does not download tools or modify the system. The test driver
retains every compiler/runner invocation. QEMU guest LD_LIBRARY_PATH must not
inherit the host LLVM libraries, especially for MIPS N32 /lib32 loaders.
"""
import argparse
import json
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--sdk-root", type=Path, required=True)
p.add_argument("--qemu-dir", type=Path, required=True)
p.add_argument("--clang", default="clang++")
p.add_argument("--ros", action="store_true")
p.add_argument("--native-rounding", action="store_true")
p.add_argument("--output", type=Path, required=True)
a = p.parse_args()
# name, SDK directory prefix, GCC triple, LLVM triple, QEMU, CPU, target flags
rows = [
    ("riscv32-glibc", "riscv32-ilp32d--glibc", "riscv32-buildroot-linux-gnu", "riscv32-linux-gnu", "riscv32", "rv32", ["-march=rv32gc", "-mabi=ilp32d"]),
    ("riscv32-musl", "riscv32-ilp32d--musl", "riscv32-buildroot-linux-musl", "riscv32-linux-musl", "riscv32", "rv32", ["-march=rv32gc", "-mabi=ilp32d"]),
    ("aarch64be", "aarch64be--glibc", "aarch64_be-buildroot-linux-gnu", "aarch64_be-linux-gnu", "aarch64_be", "max", []),
    ("armv5-soft", "armv5-eabi--glibc", "arm-buildroot-linux-gnueabi", "arm-linux-gnueabi", "arm", "arm926", ["-march=armv5te", "-mfloat-abi=soft"]),
    ("mips32r6el", "mips32r6el--glibc", "mipsel-buildroot-linux-gnu", "mipsel-linux-gnu", "mipsel", "mips32r6-generic", ["-march=mips32r6", "-mabi=32", "-mnan=2008"]),
    ("mips64-n32", "mips64-n32--glibc", "mips64-buildroot-linux-gnu", "mips64-linux-gnuabin32", "mipsn32", "MIPS64R2-generic", ["-mabi=n32", "-march=mips64r2"]),
    ("mips64el-n32", "mips64el-n32--glibc", "mips64el-buildroot-linux-gnu", "mips64el-linux-gnuabin32", "mipsn32el", "MIPS64R2-generic", ["-mabi=n32", "-march=mips64r2"]),
]
configs = []
for name, directory, gcc, triple, qemu, cpu, flags in rows:
    sdk = a.sdk_root.resolve() / (directory + "--stable-2026.08-1")
    sysroot = sdk / gcc / "sysroot"
    runner = [str(a.qemu_dir.resolve() / ("qemu-" + qemu)), "-cpu", cpu, "-L", str(sysroot),
              "-E", "LD_LIBRARY_PATH=" + str(sysroot / "usr/lib") + ":" + str(sysroot / "lib")]
    compiler = str(sdk / "bin" / (gcc + "-g++"))
    if a.native_rounding:
        # Legacy MIPS arithmetic needs a different normalization contract; it is
        # exercised by run_legacy_nan_cross.py, not disguised as a modern target.
        if "n32" in name:
            continue
        target = dict(name=name, triple=triple, cxx=[compiler], run=runner, input_flags=flags)
        if name.startswith("riscv32"):
            target["llc_flags"] = ["-target-abi=ilp32d", "-mattr=+m,+a,+f,+d,+c"]
            target["expected_baseline_failure"] = True
            if name.endswith("musl"):
                target["baseline_missing_symbol"] = "roundeven"
        if name.startswith("mips"):
            target["llc_flags"] = ["-mcpu=mips32r6", "-mattr=+nan2008"]
        configs.append(target)
        if name.startswith("riscv32"):
            # Execute both the feature-authorized native D path and the
            # conservative f32/f64 integer path with function features absent.
            configs.append({**target, "name": name + "-integer", "lowering_mode": "native-integer"})
        continue
    for kind in ("gcc", "clang"):
        cxflags = flags.copy()
        ldflags = ["-latomic"]
        cxx = [compiler]
        if kind == "clang":
            cxx = [a.clang, "--target=" + triple,
                   "--gcc-install-dir=" + str(sdk / "lib/gcc" / gcc / "15.3.0"),
                   "--sysroot=" + str(sysroot)]
            if name.startswith("mips"):
                cxflags += ["-mllvm", "-mips-tail-calls"]
                # These SDK CRT objects lack the GNU-stack note. Do not opt into
                # an executable stack just to satisfy lld's diagnostic: use the
                # SDK BFD linker with an explicit non-executable stack instead.
                cxx += ["--ld-path=" + str(sdk / "bin" / (gcc + "-ld"))]
                ldflags += ["-Wl,-z,noexecstack"]
            else:
                cxx += ["-fuse-ld=lld"]
        configs.append(dict(name=name + "-" + kind, cxx=cxx, run=runner, flags=cxflags,
                            ldflags=ldflags, optimizations=["O0", "O3"],
                            suites=["scalar", "boundary", "tail"] + ([] if a.ros else ["simd"])))
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(json.dumps(configs, indent=2) + "\n")

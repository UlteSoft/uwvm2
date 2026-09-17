#!/usr/bin/env python3
"""Check the production RISC-V64 address emitter using LLVM objects and QEMU.
Run within the caller's CPU/memory budget; no system packages are installed.
"""
import argparse
from itertools import product
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def inspect(assembly, relocations):
    functions = re.findall(r"^[0-9a-f]+ <(pointer_\d+)>:\n(.*?)(?=^[0-9a-f]+ <|\Z)", assembly, re.M | re.S)
    assert len(functions) == 270, len(functions)
    allowed = {"addi", "addiw", "slli", "lui", "li", "ret", "mv", "not", "xori", "srli",
               "bseti", "bclri", "slli.uw", "add.uw", "zext.w"}
    counts = []
    for name, body in functions:
        instructions = re.findall(r"^\s*[0-9a-f]+:\s+([a-z][a-z0-9.]*)\b([^\n]*)", body, re.M)
        assert instructions and instructions[-1][0] == "ret", (name, body)
        assert len(instructions) <= 9, (name, body)
        assert all(op in allowed and not re.search(r"\bsp\b|\(|\)", args) for op, args in instructions), (name, body)
        assert "R_RISCV" not in body, (name, body)
        counts.append(len(instructions))
    # Unwind metadata may have its own PC-relative relocations, but the text
    # containing host-address load/store/call must need no address fixups.
    assert not re.search(r"Section[^\n]*\.rela?\.text\b", relocations), relocations
    return dict(addresses=270, stack_accesses=0, constant_pool_loads=0,
                native_calls_in_address_materializers=0, text_relocations=0,
                max_instructions_including_ret=max(counts))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "clang++"))
    parser.add_argument("--llvm-config", default=os.environ.get("LLVM_CONFIG", "llvm-config"))
    parser.add_argument("--riscv-cc", default=os.environ.get("RISCV_CC", "riscv64-linux-gnu-gcc"))
    parser.add_argument("--riscv-sysroot", default=os.environ.get("RISCV_SYSROOT"))
    parser.add_argument("--qemu", default=os.environ.get("QEMU_RISCV64", "qemu-riscv64"))
    parser.add_argument("--out", type=Path, help="new output directory; must not already exist")
    args = parser.parse_args()
    if args.out:
        out = args.out.resolve()
        out.mkdir(parents=True, exist_ok=False)
    else:
        out = Path(tempfile.mkdtemp(prefix="uwvm-riscv-address-"))
    commands = []

    def run(command, *, timeout=120):
        commands.append([str(item) for item in command])
        result = subprocess.run(commands[-1], capture_output=True, text=True, timeout=timeout)
        (out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        if result.returncode:
            raise RuntimeError(f"{shlex.join(commands[-1])}\nexit={result.returncode}\n{result.stdout}{result.stderr}")
        return result

    def llvm_query(*arguments):
        return run([args.llvm_config, *arguments]).stdout.strip()

    bindir = Path(llvm_query("--bindir"))
    libdir = llvm_query("--libdir")
    cxxflags = shlex.split(llvm_query("--cxxflags"))
    linkflags = shlex.split(llvm_query("--ldflags", "--libs", "core", "--system-libs"))
    generator = out / "generate"
    run([args.cxx, *cxxflags, "-std=c++23", "-O2", f"-I{ROOT / 'src'}",
         ROOT / "test/0014.llvm_jit/llvm_jit_riscv_host_address.cc",
         *linkflags, f"-Wl,-rpath,{libdir}", "-o", generator])
    run([generator])
    source = out / "input.ll"
    source.write_text(run([generator, out / "runner.c"]).stdout)
    optimized = out / "optimized.ll"
    run([bindir / "opt", "-passes=default<O3>", "-S", source, "-o", optimized])
    records = []
    # `li` expansion is feature-dependent. Check uncompressed RV64G, RV64GC,
    # and the bit-manipulation instructions used on newer native hosts.
    features = {
        "rv64g": "+m,+a,+f,+d,-c,-zba,-zbb,-zbs",
        "rv64gc": "+m,+a,+f,+d,+c,-zba,-zbb,-zbs",
        "rv64gc-zb": "+m,+a,+f,+d,+c,+zba,+zbb,+zbs",
    }
    for isa, level, relocation in product(features, (0, 3), ("static", "pic")):
        name = f"{isa}-O{level}-{relocation}"
        obj = out / f"{name}.o"
        run([bindir / "llc", f"-O{level}", f"-relocation-model={relocation}",
             "-mtriple=riscv64-unknown-linux-gnu", f"-mattr={features[isa]}",
             "-filetype=obj", source if level == 0 else optimized, "-o", obj])
        assembly = run([bindir / "llvm-objdump", "-dr", "--no-show-raw-insn", obj]).stdout
        relocations = run([bindir / "llvm-readobj", "--relocations", "--sections", obj]).stdout
        (out / f"{name}.s").write_text(assembly)
        (out / f"{name}.sections").write_text(relocations)
        record = inspect(assembly, relocations)
        binary = out / f"{name}.test"
        sysroot = [f"--sysroot={args.riscv_sysroot}"] if args.riscv_sysroot else []
        run([args.riscv_cc, *sysroot, "-O2", "-static", out / "runner.c", obj, "-o", binary])
        executed = run([args.qemu, binary], timeout=60)
        (out / f"{name}.log").write_text(executed.stdout + executed.stderr)
        assert "PASS 270 full-width pointers and >4 GiB load/store/call" in executed.stdout
        record.update(mode=name, features=features[isa], high_address_load_store_call=True)
        records.append(record)
    (out / "results.json").write_text(json.dumps(records, indent=2) + "\n")
    print(json.dumps(dict(profiles=len(records), failed=0, out=str(out))))


if __name__ == "__main__":
    main()

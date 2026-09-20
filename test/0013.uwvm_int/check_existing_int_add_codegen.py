#!/usr/bin/env python3
"""Inspect selected scalar/SIMD add opfuncs in an existing x86-64 ELF binary.

This checks actual linked interpreter specializations, including named-module
symbols, not a separately optimized helper fixture. It deliberately requires
SSE/AVX native adds and tail dispatch; it is not a no-SSE or cross-ISA oracle.
Any RBP/RSP mention is conservatively flagged for review: RBP could be an
ordinary allocated register, so failure alone does not prove a native spill.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def select(symbol):
    # Named-module demangling may annotate the enum's owner/name. Match the
    # enum value up to its template-argument delimiter, not an exact header-only
    # spelling. Never accept an empty selection as an assembly test pass.
    def function(name):
        # Clang annotates the function itself too, not just the enum type:
        # uwvmint_f32_binop@module.name:partition<(float_binop@module)0, ...>
        return re.search(r"::"+name+r"(?:@[\w.:]+)?<", symbol)
    scalar_add = re.search(r"::float_binop[^,]*\)0,", symbol)
    if function("uwvmint_f32_binop") and scalar_add:
        return "f32-add", {"addss", "addps", "vaddss", "vaddps"}
    if function("uwvmint_f64_binop") and scalar_add:
        return "f64-add", {"addsd", "addpd", "vaddsd", "vaddpd"}
    if function("uwvmint_simd_v128_binop") and re.search(r"::v128_binop[^,]*\)8,", symbol):
        return "f32x4-add", {"addps", "vaddps"}
    if function("uwvmint_simd_full_binop"):
        if re.search(r"::op_simd[^,]*\)228,", symbol):
            return "f32x4-add", {"addps", "vaddps"}
        if re.search(r"::op_simd[^,]*\)240,", symbol):
            return "f64x2-add", {"addpd", "vaddpd"}
    return None


def inspect(instructions, native_ops):
    calls = [op for op, _ in instructions if op.startswith("call")]
    stack_registers = any(re.search(r"\b(?:rsp|rbp|esp|ebp)\b", operands)
                          for _, operands in instructions)
    # PUSH/POP/RET change the native stack without naming RSP. A local direct
    # jump is not evidence of interpreter tail dispatch either.
    implicit_stack = [op for op, _ in instructions
                      if op.startswith(("push", "pop", "ret", "enter", "leave"))]
    native_add = any(op in native_ops for op, _ in instructions)
    tail_dispatch = any(op.startswith("jmp") and "*" in operands for op, operands in instructions)
    passed = bool(instructions) and not calls and not stack_registers and not implicit_stack and native_add and tail_dispatch
    return dict(calls=len(calls), stack_register_mentioned=stack_registers,
                implicit_stack_ops=implicit_stack, native_add=native_add,
                tail_dispatch=tail_dispatch, passed=passed)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--llvm-nm", required=True)
    parser.add_argument("--llvm-objdump", required=True)
    args = parser.parse_args()
    binary = args.binary.resolve()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    with binary.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    nm_command = [args.llvm_nm, "-S", "-C", "--defined-only", str(binary)]
    symbols = subprocess.check_output(nm_command, text=True, timeout=90)
    found = dict.fromkeys(("f32-add", "f64-add", "f32x4-add", "f64x2-add"), 0)
    rows = []
    for line in symbols.splitlines():
        match = re.fullmatch(r"([0-9a-f]+) ([0-9a-f]+) [A-Za-z] (.*)", line)
        if not match:
            continue
        address, size, symbol = match.groups()
        selected = select(symbol)
        if not selected:
            continue
        group, native_ops = selected
        found[group] += 1
        command = [args.llvm_objdump, "-d", "--no-show-raw-insn",
                   "--start-address=0x"+address,
                   "--stop-address="+hex(int(address, 16)+int(size, 16)), str(binary)]
        assembly = subprocess.check_output(command, text=True, timeout=90)
        (out/(group+"-"+str(found[group])+".s")).write_text(assembly)
        instructions = re.findall(r"^\s*[0-9a-f]+:\s+([a-z][a-z0-9.]*)\b([^\n]*)", assembly, re.M)
        rows.append(dict(group=group, symbol=symbol, command=command,
                         instructions=len(instructions), **inspect(instructions, native_ops)))
    report = dict(note=__doc__, binary=str(binary), binary_sha256=digest,
                  checker_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  nm_command=nm_command, selected_groups=found, functions=rows,
                  passed=all(found.values()) and all(row["passed"] for row in rows))
    (out/"results.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(dict(groups=found, functions=len(rows), passed=report["passed"])))
    raise SystemExit(not report["passed"])


if __name__ == "__main__":
    main()

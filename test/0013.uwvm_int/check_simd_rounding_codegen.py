#!/usr/bin/env python3
"""Run the shared SIMD rounding regression and inspect native x86-64 code.

Use an SSE4.1-capable x86-64 host. This is a shared evaluator test, not a
whole-interpreter or LLVM-JIT build. The output directory must be new.
"""
import argparse
import json
import pathlib
import platform
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--objdump", required=True, help="llvm-objdump")
    parser.add_argument("--out", type=pathlib.Path, required=True)
    parser.add_argument("--source-root", type=pathlib.Path, help="fixed full source snapshot")
    parser.add_argument("--include-overlay", type=pathlib.Path, help="override selected headers only")
    args = parser.parse_args()
    if platform.machine().lower() not in ("x86_64", "amd64"):
        print("SKIP: native x86-64 required")
        return 77
    args.out.mkdir(parents=True, exist_ok=False)
    repo = args.source_root or pathlib.Path(__file__).resolve().parents[2]
    commands = []

    def run(command, label):
        commands.append(command)
        (args.out / "commands.json").write_text(json.dumps(commands, indent=2))
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=180)
        (args.out / (label + ".log")).write_text(result.stdout)
        if result.returncode:
            raise RuntimeError(f"{label}: exit {result.returncode}")
        return result.stdout

    common = [args.cxx, "-std=c++26", "-O3", "-fno-exceptions", "-fno-rtti",
              "-fno-math-errno", "-ffp-contract=off", "-DUWVM_USE_THREAD_LOCAL"]
    if args.include_overlay:
        common.append("-I" + str(args.include_overlay))
    for include in ("src", "third-parties/fast_io/include", "third-parties/bizwen/include",
                    "third-parties/boost_unordered/include"):
        common.append("-I" + str(repo / include))
    source = str(pathlib.Path(__file__).resolve().with_name("uwvm_int_simd_rounding.cc"))
    records = []
    for profile, feature in (("sse2", "-mno-sse4.1"), ("sse41", "-msse4.1")):
        binary = args.out.resolve() / profile
        run(common + ["-march=x86-64", feature, source, "-o", str(binary)], profile + "-compile")
        output = run([str(binary)], profile + "-run")
        if "checks=792 failures=0" not in output:
            raise RuntimeError(f"{profile}: incomplete runtime regression")
        assembly = run([args.objdump, "-d", "--no-show-raw-insn", str(binary)], profile + "-asm")
        functions = dict(re.findall(r"^[0-9a-f]+ <([^>]+)>:\n(.*?)(?=^[0-9a-f]+ <|\Z)",
                                    assembly, re.M | re.S))
        for width in (32, 64):
            for operation in ("ceil", "floor", "trunc", "nearest"):
                name = f"round_f{width}_{operation}"
                body = functions[name]
                instructions = re.findall(r"^\s*[0-9a-f]+:\s+([a-z][a-z0-9.]*)\b([^\n]*)", body, re.M)
                if not instructions or any(op.startswith("call") for op, _ in instructions):
                    raise RuntimeError(f"{profile}/{name}: empty body or native helper call")
                packed = "roundps" if width == 32 else "roundpd"
                count = sum(op == packed for op, _ in instructions)
                if profile == "sse41":
                    if count != 1 or re.search(r"\b(?:rsp|esp|rbp|ebp)\b", body):
                        raise RuntimeError(f"{profile}/{name}: not one stack-free packed rounding")
                records.append({"profile": profile, "function": name,
                                "instructions": len(instructions), "packed_rounds": count})
    (args.out / "results.json").write_text(json.dumps({"checks": 1584, "failures": 0,
                                                      "functions": records}, indent=2))
    print("PASS 1584 runtime checks; 16 helper-free functions; 8 packed SSE4.1 roundings")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

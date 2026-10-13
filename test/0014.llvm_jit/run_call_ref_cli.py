#!/usr/bin/env python3
"""Execute new Core 3 call_ref/return_call_ref syntax in LLVM modes and Wasmtime.

The positive guests assert their results. The tail case uses a long chain so a
call-plus-return implementation runs out of native stack. All VM runs require
the Linux test cgroup and a source-matching built binary.
"""
import argparse
import hashlib
import json
import resource
import subprocess
from pathlib import Path

from run_wasm3_multi_memory import configurations


CASES = {
    "direct": ("success", """(module
      (type $t (func (param i32) (result i32)))
      (func $add (type $t) (param i32) (result i32) local.get 0 i32.const 2 i32.add)
      (elem declare func $add)
      (func (export "_start")
        i32.const 40 ref.func $add call_ref $t
        i32.const 42 i32.ne if unreachable end))"""),
    "duplicate-structural-type": ("success", """(module
      (type $original (func (param i32) (result i32)))
      (type $duplicate (func (param i32) (result i32)))
      (func $add (type $original) (param i32) (result i32) local.get 0 i32.const 3 i32.add)
      (elem declare func $add)
      (func (export "_start")
        i32.const 39 ref.func $add call_ref $duplicate
        i32.const 42 i32.ne if unreachable end))"""),
    "multi-result": ("success", """(module
      (type $t (func (param i32) (result i32 i64)))
      (func $pair (type $t) (param i32) (result i32 i64)
        local.get 0 i32.const 1 i32.add i64.const 99)
      (elem declare func $pair)
      (func (export "_start")
        i32.const 41 ref.func $pair call_ref $t
        i64.const 99 i64.ne if unreachable end
        i32.const 42 i32.ne if unreachable end))"""),
    "null-concrete": ("trap", """(module
      (type $t (func (param i32) (result i32)))
      (func (export "_start")
        i32.const 41 ref.null $t call_ref $t drop))"""),
    "caught-exception": ("success", """(module
      (type $t (func)) (tag $e)
      (func $throw (type $t) throw $e) (elem declare func $throw)
      (func (export "_start")
        block $out
          try_table (catch $e $out)
            ref.func $throw call_ref $t unreachable
          end
          unreachable
        end))"""),
    "tail-deep": ("success", """(module
      (type $t (func (param i32) (result i32)))
      (func $walk (type $t) (param i32) (result i32)
        local.get 0 i32.eqz if i32.const 17 return end
        local.get 0 i32.const 1 i32.sub ref.func $walk return_call_ref $t)
      (elem declare func $walk)
      (func (export "_start")
        i32.const 250000 call $walk i32.const 17 i32.ne if unreachable end))"""),
    "erased-funcref-rejected": ("validation", """(module
      (type $t (func))
      (func (export "_start") ref.null func call_ref $t))"""),
    "wrong-ref-func-type-rejected": ("validation", """(module
      (type $t (func (param i32))) (type $u (func))
      (func $f (type $u)) (elem declare func $f)
      (func (export "_start") i32.const 1 ref.func $f call_ref $t))"""),
    "wrong-tail-result-rejected": ("validation", """(module
      (type $t (func (result i32)))
      (func $f (type $t) i32.const 3) (elem declare func $f)
      (func (export "_start") ref.func $f return_call_ref $t))"""),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--uwvm", type=Path)
    parser.add_argument("--wasmtime", type=Path)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--configuration", help="one configuration label, e.g. jit-full-unwind")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.output.mkdir(parents=True, exist_ok=False)
    cases = []
    for name, (expected, wat) in CASES.items():
        source = args.output / (name + ".wat")
        binary = source.with_suffix(".wasm")
        source.write_text(wat)
        subprocess.run([str(args.wasm_tools), "parse", str(source), "-o", str(binary)], check=True)
        cases.append((name, expected, binary))
    configs = []
    if args.uwvm:
        configs = [(name, [x for x in command[:-1] if x != "-WFE-multi-memory"])
                   for name, command in configurations(args.uwvm.resolve(), args.ros)
                   if name.startswith(("jit-", "tiered-"))]
    if args.wasmtime:
        configs.append(("wasmtime", [str(args.wasmtime.resolve()), "-C", "cache=n",
                                     "-W", "function-references=y", "-W", "tail-call=y"]))
    if args.configuration:
        configs = [(name, command) for name, command in configs if name == args.configuration]
    if not configs:
        raise RuntimeError("no matching runtime configurations")
    rows = []
    for label, base in configs:
        for name, expected, binary in cases:
            exception_case = name == "caught-exception"
            flags = (["-W", "exceptions=y"] if exception_case else []) if label == "wasmtime" else [
                "-WFE-function-references", "-WFE-tail-call",
                *(["-WFE-exceptions"] if exception_case else []), "--run"]
            command = base + flags + [str(binary)]
            run = subprocess.run(command, capture_output=True, timeout=120)
            output = run.stdout + run.stderr
            (args.output / (label + "-" + name + ".log")).write_bytes(output)
            passed = (run.returncode == 0) == (expected == "success")
            if expected == "trap" and label != "wasmtime":
                passed = passed and b"null reference" in output.lower() and b"Call stack:" in output
            if expected == "validation":
                passed = passed and any(word in output.lower() for word in
                                        (b"validat", b"type mismatch", b"invalid input webassembly code"))
            rows.append(dict(configuration=label, case=name, expected=expected,
                             exit=run.returncode, passed=passed, command=command))
            (args.output / "runs.json").write_text(json.dumps(rows, indent=2) + "\n")
            if not passed:
                raise RuntimeError(label + " " + name + "\n" + output.decode(errors="replace"))
        print("PASS", label, flush=True)
    gates = []
    if args.uwvm and not args.configuration:
        gate_cases = [
            ("function-references-disabled", ["-WFD-function-references"], "direct",
             b"--wasm-feature-enable-function-references"),
            ("tail-call-disabled", ["-WFE-function-references", "-WFD-tail-call"], "tail-deep",
             b"--wasm-feature-enable-tail-call"),
        ]
        for name, flags, case, diagnostic in gate_cases:
            binary = args.output / (case + ".wasm")
            command = [str(args.uwvm.resolve()), "-m", "validation", *flags, "--run", str(binary)]
            run = subprocess.run(command, capture_output=True, timeout=60)
            output = run.stdout + run.stderr
            (args.output / ("gate-" + name + ".log")).write_bytes(output)
            if run.returncode == 0 or diagnostic not in output:
                raise RuntimeError(name + "\n" + output.decode(errors="replace"))
            gates.append(dict(gate=name, exit=run.returncode, command=command))
        (args.output / "gates.json").write_text(json.dumps(gates, indent=2) + "\n")
    (args.output / "summary.json").write_text(json.dumps(dict(
        passed=True, cases=len(cases), runs=len(rows), configurations=len(configs), gates=len(gates),
        binary_sha256=hashlib.sha256(args.uwvm.read_bytes()).hexdigest() if args.uwvm else None,
    ), indent=2) + "\n")


if __name__ == "__main__":
    main()

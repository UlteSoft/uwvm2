#!/usr/bin/env python3
"""Full INT execution and launch-time rejection of the LLVM-only -Rdbg entry."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
for name in ("source-root", "uwvm", "wasm-tools", "qemu-prefix-json", "out"):
    parser.add_argument("--" + name, type=Path, required=True)
parser.add_argument("--ros", action="store_true")
args = parser.parse_args()
source = args.source_root.resolve(strict=True)
binary = args.uwvm.resolve(strict=True)
out = args.out.resolve()
subprocess.run(["bash", str(source / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
out.mkdir(parents=True, exist_ok=False)
wat = out / "fixture.wat"
wasm = out / "fixture.wasm"
wat.write_text('(module (memory 1) (func (export "_start") '
               'i32.const 0 i32.const 17 i32.store '
               'i32.const 0 i32.load i32.const 41 i32.xor drop))\n')
subprocess.run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)], check=True)
subprocess.run([str(args.wasm_tools), "validate", str(wasm)], check=True)
prefix = json.loads(args.qemu_prefix_json.read_text())
mode = ["-Rint"] if args.ros else ["-Rcc", "int", "-Rcm", "full"]
rows = []
for debugging in (False, True):
    command = [*prefix, str(binary), *(["-Rdbg"] if debugging else []),
               *mode, "--run", str(wasm)]
    path = out / ("debug-rejection.log" if debugging else "normal-execution.log")
    with path.open("wb") as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=60)
    text = path.read_bytes()
    assert b"(uwvm-debug)" not in text
    for forbidden in (b"native-pc=0x", b"native instruction 0x",
                      b"native-registers stop=", b"native-disassembly stop="):
        assert forbidden not in text, (command, text)
    if debugging:
        assert result.returncode == 126, (result.returncode, text)
        assert b"debug-jit is unsupported in the current mode: uwvm-int/full" in text, text
        assert b"It requires LLVM JIT full compilation and native thread support" in text, text
    else:
        assert result.returncode == 0, (result.returncode, text)
    rows.append({"argv": command, "returncode": result.returncode, "debugging": debugging,
                 "log": str(path), "sha256": hashlib.file_digest(path.open("rb"), "sha256").hexdigest()})
summary = {"passed": True, "scope": "Actual full INT Wasm memory/scalar execution "
           "and launch-time rejection of the LLVM-only -Rdbg entry; no live "
           "debugger, Wasm single-step or JIT-native-execution qualification",
           "normal_Wasm_executions": 1, "Rdbg_startup_refusals": 1,
           "VM_native_context_exposed": False, "commands": rows,
           "binary_sha256": hashlib.file_digest(binary.open("rb"), "sha256").hexdigest(),
           "wasm_sha256": hashlib.file_digest(wasm.open("rb"), "sha256").hexdigest(),
           "cgroup": Path("/proc/self/cgroup").read_text()}
(out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
print("PASS full INT actual Wasm execution; LLVM-only -Rdbg refused at startup; "
      "VM native context hidden", flush=True)

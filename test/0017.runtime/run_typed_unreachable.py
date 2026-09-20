#!/usr/bin/env python3
"""Check MVP result-typed unreachable through pure interpreter entry paths.

Full: int-lazy and int-full. ROS: --ros selects its retained interpreter.
No native compilation; every child inherits single-CPU affinity or uses --cpu.
Core 1.0 already permits stack-polymorphic unreachable:
https://www.w3.org/TR/wasm-core-1/#valid-unreachable
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--cpu", help="Linux taskset CPU; otherwise inherit affinity")
    parser.add_argument("--wat2wasm", default="wat2wasm")
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    output = Path(tempfile.mkdtemp(prefix="uwvm-typed-unreachable-", dir="/tmp"))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    prefix = ["taskset", "-c", args.cpu] if args.cpu else []
    print(f"Artifacts: {output}", flush=True)
    modes = [("ros-int", ["-Rint"])] if args.ros else [
        ("int-lazy", ["-Rcm", "lazy", "-Rcc", "int"]),
        ("int-full", ["-Rcm", "full", "-Rcc", "int"])]
    guests = []
    for value_type in ("i32", "i64", "f32", "f64"):
        for traps in (False, True):
            name = f"{value_type}-{'trap' if traps else 'return'}"
            module = f"typed_unreachable_{value_type}"
            # A normal-return control must produce the declared value, while
            # the trap body has a true zero-byte runtime operand-stack peak.
            body = "unreachable" if traps else f"{value_type}.const 7"
            wat = f"""(module ${module}
  (func $callee (param i32) (result {value_type})
    {body})
  (func $_start (export "_start")
    i32.const 17
    call $callee
    {value_type}.const 7
    {value_type}.ne
    if unreachable end))
"""
            wat_path = output / f"{name}.wat"
            wasm_path = output / f"{name}.wasm"
            wat_path.write_text(wat)
            subprocess.run(prefix + [args.wat2wasm, str(wat_path), "--debug-names",
                           "-o", str(wasm_path)], check=True, timeout=30)
            guests.append((name, module, wasm_path, traps))
    results = []
    for mode, options in modes:
        for name, module, wasm_path, traps in guests:
            command = prefix + [str(binary), *options, "-Rct", "0",
                                "--wasip1-noinherit-system-environment",
                                "--run", str(wasm_path)]
            try:
                proc = subprocess.run(command, capture_output=True, timeout=30, check=False)
                status, stdout, stderr = proc.returncode, proc.stdout, proc.stderr
            except subprocess.TimeoutExpired as error:
                status, stdout, stderr = 124, error.stdout or b"", error.stderr or b""
            stem = output / f"{mode}-{name}"
            stem.with_suffix(".stdout").write_bytes(stdout)
            stem.with_suffix(".stderr").write_bytes(stderr)
            stem.with_suffix(".command.json").write_text(json.dumps(command, indent=2) + "\n")
            log = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", stderr.decode(errors="replace"))
            frames = [(int(index), label, int(function)) for index, label, function in
                      re.findall(r'^uwvm: \[info\]\s+#(\d+) module=(.*?) func_idx=(\d+)(?: func_name="[^"]*")?\s*$',
                                 log, re.MULTILINE)]
            expected = [(0, module, 0), (1, module, 1)]
            diagnosed = "Runtime crash (catch unreachable)" in log and frames == expected
            correct = (status != 0 and status != 124 and diagnosed) if traps else status == 0
            item = dict(name=f"{mode}-{name}", status=status, passed=correct and stdout == b"",
                        expected_trap=traps, frames=frames,
                        stderr_sha256=hashlib.sha256(stderr).hexdigest())
            results.append(item)
            print(json.dumps(item), flush=True)
    (output / "summary.json").write_text(json.dumps(results, indent=2) + "\n")
    raise SystemExit(0 if all(item["passed"] for item in results) else 1)


if __name__ == "__main__":
    main()

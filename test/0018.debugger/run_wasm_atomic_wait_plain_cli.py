#!/usr/bin/env python3
"""Check ordinary full-JIT wait/notify results with debugging disabled."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def fixture(address, compare):
    addr = "i64" if address == 64 else "i32"
    comp = "i64" if compare == 64 else "i32"
    return f'''(module
  (memory {"i64 " if address == 64 else ""}1 1 shared)
  (data ({addr}.const 16) "\\07\\00\\00\\00\\00\\00\\00\\00")
  (func (export "_start")
    {addr}.const 16 {comp}.const 6 i64.const -1 memory.atomic.wait{compare}
    i32.const 1 i32.ne if unreachable end
    {addr}.const 16 {comp}.const 7 i64.const 1000000 memory.atomic.wait{compare}
    i32.const 2 i32.ne if unreachable end
    {addr}.const 16 i32.const 2 memory.atomic.notify
    i32.eqz if else unreachable end))
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source-root", "binary", "wasm-tools", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--runner-prefix-json", type=Path)
    parser.add_argument("--ros", action="store_true")
    args = parser.parse_args()
    subprocess.run(["bash", str(args.source_root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    args.out.mkdir(parents=True, exist_ok=False)
    prefix = json.loads(args.runner_prefix_json.read_text()) if args.runner_prefix_json else []
    assert isinstance(prefix, list) and all(isinstance(s, str) and s and "\0" not in s for s in prefix)
    rows = []
    for address in (32, 64):
        for compare in (32, 64):
            wat = args.out / f"plain-{address}-{compare}.wat"
            wasm = wat.with_suffix(".wasm")
            wat.write_text(fixture(address, compare))
            subprocess.run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)], check=True)
            subprocess.run([str(args.wasm_tools), "validate", "--features", "all", str(wasm)], check=True)
            for policy in ("instruction", "unwind"):
                mode = ["-Raot"] if args.ros else ["-Rcc", "jit", "-Rcm", "full"]
                argv = prefix + [str(args.binary), *mode, "-Rct", "0", "-Rllvm-cache-path", "disable",
                                 "-Rllvm-call-stack", policy, "-WFE-threads", "-WFE-memory64", "--run", str(wasm)]
                row = dict(address_bits=address, compare_bits=compare, policy=policy, argv=argv,
                           actual_VM=True, debugging_enabled=False,
                           asserted_results=dict(not_equal=1, timed_out=2, notify_without_waiters=0),
                           fixture_sha256=hashlib.sha256(wasm.read_bytes()).hexdigest())
                try:
                    # The outer owned-tree supervisor retires a timed-out child;
                    # TimeoutExpired/nonzero/signal can never count as a pass.
                    done = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
                    log = args.out / f"plain-{address}-{compare}-{policy}.log"
                    log.write_bytes(done.stdout)
                    row.update(exit_code=done.returncode, passed=done.returncode == 0,
                               log_sha256=hashlib.sha256(done.stdout).hexdigest())
                except Exception as error:
                    row.update(passed=False, error=repr(error))
                rows.append(row)
                (args.out / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
                print(json.dumps(row), flush=True)
    inputs = dict(binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(),
                  script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), runner_prefix=prefix)
    if prefix:
        inputs["runner_sha256"] = hashlib.sha256(Path(prefix[0]).read_bytes()).hexdigest()
    (args.out / "inputs.json").write_text(json.dumps(inputs, indent=2) + "\n")
    raise SystemExit(0 if rows and all(row["passed"] for row in rows) else 1)


if __name__ == "__main__":
    main()

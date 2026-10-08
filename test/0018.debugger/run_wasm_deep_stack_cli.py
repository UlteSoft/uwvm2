#!/usr/bin/env python3
"""Real generated deep activations, typed pages, opcode stepping and native scope.

Run only under the Linux keeper cgroup. No DWARF or native stack guesses.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import resource
import subprocess
import time
from run_wasm_operand_preview_cli import OperandConsole, markers, operands


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source-root", "binary", "wasm-tools", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--runner-prefix-json", type=Path,
                        help="JSON argv prefix for the actual QEMU target VM")
    parser.add_argument("--total", type=int, action="append",
                        help="Select actual activation depths; defaults to the full boundary matrix")
    parser.add_argument("--prompt-timeout", type=int, default=120)
    parser.add_argument("--stop-timeout", type=int, default=30)
    args = parser.parse_args()
    runner = json.loads(args.runner_prefix_json.read_text()) if args.runner_prefix_json else []
    assert isinstance(runner, list) and all(isinstance(x, str) and x and "\0" not in x for x in runner)
    assert 1 <= args.prompt_timeout <= 600 and 1 <= args.stop_timeout <= 120
    OperandConsole.prompt_timeout = args.prompt_timeout
    totals = args.total or (63, 64, 65, 128, 1024, 4097, 10000)
    assert len(set(totals)) == len(totals) and all(4 <= total <= 10000 for total in totals)
    subprocess.run(["bash", str(args.source_root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_STACK, (64 << 20, 64 << 20))
    args.out.mkdir(parents=True, exist_ok=False)
    spec = importlib.util.spec_from_file_location("real_deep_dap_parser", args.source_root / "tools/debug/dap_adapter.py")
    dap = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(dap)
    rows = []
    for total in totals:
        depth = total - 3
        wat = args.out / f"depth-{total}.wat"
        wasm = wat.with_suffix(".wasm")
        wat.write_text(f'''(module
          (func $bottom (param i32) (result i64)
            i32.const -7 i64.const 123456789 nop drop drop i64.const 0)
          (func $recurse (param $n i32) (result i64)
            local.get $n if (result i64)
              local.get $n i64.const 123456789
              local.get $n i32.const 1 i32.sub call $recurse
              drop drop drop i64.const 1
            else local.get $n call $bottom end)
          (func (export "_start") i32.const {depth} call $recurse drop)
          (func (result i64) i64.const 0))\n''')
        subprocess.run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)], check=True)
        subprocess.run([str(args.wasm_tools), "validate", "--features", "all", str(wasm)], check=True)
        sites, dump = markers(args.wasm_tools, wasm)
        wat.with_suffix(".dump").write_text(dump)
        for policy in (("instruction", "unwind", "instruction-native") if total == 4097 else ("instruction", "unwind")):
            name = f"depth-{total}-{policy}"
            console = OperandConsole([*runner, str(args.binary), "-Rdbg", "-Rct", "0", "-Rllvm-cache-path", "disable",
                                      "-Rllvm-call-stack", policy.split("-")[0], "--run", str(wasm)], args.out / (name + ".log"))
            row = {"total": total, "policy": policy, "actual_VM": True, "observations": []}
            try:
                console.prompt()
                assert b"registered" in console.send(f"break 0 0 {sites[0][0]}")
                console.send("continue")
                deadline = time.monotonic() + args.stop_timeout
                while b"stopped:" not in console.send("status"):
                    assert time.monotonic() < deadline
                    time.sleep(.01)
                thread, function, offset = console.location()
                assert (function, offset) == (0, sites[0][0])
                status = console.send("status").removesuffix(b"(uwvm-debug) ").decode()
                stop = dap.parse_status(status)[2][0]["stop_id"]
                for first in (0, 63, 64, 127, 1023, 4096, total - 1, total):
                    page_text = console.send(f"frames wasm {thread} {stop} {first} 3").removesuffix(b"(uwvm-debug) ").decode()
                    page = dap.parse_source_frames(page_text, thread, stop, with_page=True, wasm=True)
                    assert page["total"] == total and page["first"] == min(first, total), page
                    assert len(page["rows"]) == min(3, total - page["first"]), page
                operands(console, thread, 0, ["i32 = -7", "i64 = 123456789"], row["observations"])
                for frame in (2, min(64, total - 2), min(4096, total - 2), total - 2):
                    operands(console, thread, frame, [f"i32 = {frame - 1}", "i64 = 123456789"], row["observations"])
                # A genuine active ancestor must block replacement beyond the old trace capacity.
                replacement = args.out / (name + ".bin")
                replacement.write_bytes(b"\x00\x42\x17\x0b")
                console.child.stdin.write(f"replace 0 1 1 {replacement}\n".encode()); console.child.stdin.flush()
                response = console.prompt()
                assert b"active" in response, response
                # Root is beyond the legacy 64-row trace at deep stops. A valid
                # inactive target must still be replaceable at the same stop.
                root_replacement = args.out / (name + "-root.bin")
                root_replacement.write_bytes(b"\x00\x0b")
                console.child.stdin.write(f"replace 0 2 1 {root_replacement}\n".encode()); console.child.stdin.flush()
                response = console.prompt()
                assert b"active" in response, response
                # Replacement needs the complete cooperative Wasm stop, before
                # a native instruction pause invalidates that cohort witness.
                response = console.send(f"replace 0 3 1 {replacement}")
                assert b"committed" in response or b"replaced" in response, response
                if policy.endswith("-native"):
                    console.send("disable 1")
                    reply = console.send(f"step asm {thread}")
                    assert b"native instruction" in reply, reply
                    status = console.send("status")
                    assert b"native-pc=" in status and b"function=0" in status, status
                    console.child.stdin.write(f"operands {thread} 0 0 64\n".encode()); console.child.stdin.flush()
                    response = console.prompt()
                    assert b"Wasm state unavailable" in response and b"operand 0 " not in response, response
                else:
                    console.send(f"step wasm {thread} over")
                    deadline = time.monotonic() + args.stop_timeout
                    while b"stopped:" not in console.send("status"):
                        assert time.monotonic() < deadline
                        time.sleep(.01)
                    assert console.location()[1] == 0
                    console.send(f"step wasm {thread} out")
                    deadline = time.monotonic() + args.stop_timeout
                    while b"stopped:" not in console.send("status"):
                        assert time.monotonic() < deadline
                        time.sleep(.01)
                    assert console.location()[1] == 1
                console.send("continue")
                deadline = time.monotonic() + args.stop_timeout
                while True:
                    reply = console.send("status")
                    if b"guest exited:" in reply:
                        assert b"guest exited: 0" in reply, reply
                        break
                    assert time.monotonic() < deadline, reply
                    time.sleep(.01)
                row["passed"] = True
            except Exception as error:
                row.update(passed=False, error=repr(error))
            finally:
                console.close()
            rows.append(row)
            (args.out / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
            print(json.dumps({k: v for k, v in row.items() if k != "observations"}), flush=True)
            if not row["passed"]:
                raise AssertionError(row)
    (args.out / "inputs.json").write_text(json.dumps({"binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
        "harness_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "runner_prefix": runner, "runner_sha256": hashlib.sha256(Path(runner[0]).read_bytes()).hexdigest() if runner else None,
        "prompt_timeout": args.prompt_timeout, "stop_timeout": args.stop_timeout, "totals": list(totals), "cgroup": Path("/proc/self/cgroup").read_text()}, indent=2) + "\n")


if __name__ == "__main__":
    main()

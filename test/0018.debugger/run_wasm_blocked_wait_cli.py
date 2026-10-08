#!/usr/bin/env python3
"""Real -Rdbg pause/query/mutate/resume of in-flight waits; Linux cgroup only."""
import argparse
import hashlib
import json
import re
import subprocess
import time
from pathlib import Path

from run_wasm_opcode_step_cli import Console, uleb


class WaitConsole(Console):
    def send(self, command):
        # Refusal is an expected result for asm commands at a VM wait bridge.
        self.transcript.extend(b"\n>>> " + command.encode() + b"\n")
        self.child.stdin.write(command.encode() + b"\n")
        self.child.stdin.flush()
        return self.prompt()


def fixture(address, compare, infinite=False):
    addr = "i64" if address == 64 else "i32"
    comp = "i64" if compare == 64 else "i32"
    return f'''(module
  (memory {"i64 " if address == 64 else ""}1 1 shared)
  (data ({addr}.const 16) "\\07\\00\\00\\00\\00\\00\\00\\00")
  (func (export "_start") (local $result i32) (local $tag i64)
    i64.const 1234 local.set $tag local.get $tag
    {addr}.const 16 {comp}.const 7 i64.const {-1 if infinite else 1000000000}
    memory.atomic.wait{compare}
    local.set $result drop))
'''


def wait_sites(wasm, compare):
    data = wasm.read_bytes()
    assert data[:8] == b"\0asm\1\0\0\0"
    at = 8
    while at < len(data):
        kind = data[at]
        size, at = uleb(data, at + 1)
        end = at + size
        assert end <= len(data)
        if kind == 10:
            count, at = uleb(data, at)
            assert count == 1
            length, at = uleb(data, at)
            body = data[at:at + length]
            groups, cursor = uleb(body, 0)
            for _ in range(groups):
                _, cursor = uleb(body, cursor)
                assert body[cursor] in (0x7f, 0x7e)
                cursor += 1
            expression = body[cursor:]
            marker = bytes((0xfe, 1 if compare == 32 else 2))
            # This deliberately small fixture has no marker-like constant LEB.
            assert expression.count(marker) == 1
            before = expression.index(marker)
            alignment, after = uleb(expression, before + 2)
            offset, after = uleb(expression, after)
            assert alignment == (2 if compare == 32 else 3) and offset == 0
            assert expression[after] == 0x21  # local.set sees the real wait result
            return before, after
        at = end
    raise AssertionError("fixture has no real code section")


def stopped(console, offset):
    until = time.monotonic() + 15
    while True:
        answer = console.send("status")
        if b"stopped:" in answer:
            location = re.search(rb"thread ([0-9]+) module=0 function=0 byte-offset=([0-9]+) generation=", answer)
            stop = re.search(rb"^stop-id ([0-9]+)$", answer, re.M)
            assert location and stop and int(location[2]) == offset, answer
            return int(location[1]), int(stop[1])
        assert b"guest exited:" not in answer and time.monotonic() < until, answer
        time.sleep(.01)


def operands(console, thread, expected):
    answer = console.send(f"operands {thread} 0 0 8")
    assert b"error:" not in answer and b"Wasm state unavailable:" not in answer, answer
    assert b"Note: Last Wasm safepoint snapshot; may differ from current native state." in answer, answer
    rows = re.findall(rb"^operand ([0-9]+) (i32|i64) = (-?[0-9]+)", answer, re.M)
    actual = [(int(index), kind.decode(), int(value)) for index, kind, value in rows]
    assert actual == [(i, kind, value) for i, (kind, value) in enumerate(expected)], answer
    return answer.decode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source-root", "binary", "wasm-tools", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--runner-prefix-json", type=Path)
    parser.add_argument("--scenario", choices=("resume", "quit"), default="resume")
    args = parser.parse_args()
    args.out.mkdir(exist_ok=False, parents=True)
    subprocess.run(["bash", str(args.source_root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    prefix = json.loads(args.runner_prefix_json.read_text()) if args.runner_prefix_json else []
    rows = []
    for address in (32, 64):
        for compare in (32, 64):
            wat = args.out / f"wait-{address}-{compare}.wat"
            wasm = wat.with_suffix(".wasm")
            wat.write_text(fixture(address, compare, args.scenario == "quit"))
            subprocess.run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)], check=True)
            subprocess.run([str(args.wasm_tools), "validate", "--features", "all", str(wasm)], check=True)
            before, after = wait_sites(wasm, compare)
            for policy in ("instruction", "unwind"):
                console = None
                row = dict(address_bits=address, compare_bits=compare, policy=policy, scenario=args.scenario, actual_VM=True,
                           before_wait_offset=before, after_wait_offset=after)
                try:
                    argv = prefix + [str(args.binary), "-Rdbg", "-Rct", "0", "-Rllvm-cache-path", "disable",
                                     "-Rllvm-call-stack", policy, "-WFE-threads", "-WFE-memory64", "--run", str(wasm)]
                    console = WaitConsole(argv, args.out / f"wait-{address}-{compare}-{policy}.log")
                    console.prompt()
                    assert b"prepared; no Wasm instruction executed" in console.send("status")
                    assert b"registered" in console.send(f"break 0 0 {before}")
                    console.send("continue")
                    thread, entry_stop = stopped(console, before)
                    before_values = [("i64", 1234), ("i64" if address == 64 else "i32", 16),
                                     ("i64" if compare == 64 else "i32", 7), ("i64", -1 if args.scenario == "quit" else 1000000000)]
                    operands(console, thread, before_values)
                    # Positive ownership control: disassembly works at the real
                    # generated pre-opcode landing, before entering the VM wait.
                    native = console.send(f"disassemble {thread} {entry_stop} 2")
                    assert b"native-disassembly stop=" in native and b"origin=safepoint-code-view" in native, native
                    console.send("delete 1")
                    console.send("continue")
                    time.sleep(.05)
                    paused = console.send("pause")
                    assert b"timed out" not in paused and b"error:" not in paused, paused
                    waited_thread, wait_stop = stopped(console, before)
                    assert waited_thread == thread and wait_stop > entry_stop
                    row["in_flight_operands"] = operands(console, thread, before_values)
                    for command in (f"disassemble {thread} {wait_stop} 2", f"step asm {thread}", f"finish asm {thread}"):
                        refusal = console.send(command)
                        assert (b"unavailable" in refusal or b"error: command or thread is not valid in the current execution state" in refusal) and b"native-disassembly stop=" not in refusal and b"native instruction 0x" not in refusal, (command, refusal)
                        assert stopped(console, before) == (thread, wait_stop)
                    if args.scenario == "resume":
                        edited = console.send(f"set wasm memory 0 0 {thread} 16 bytes 09")
                        assert b"applied=1" in edited and b"error:" not in edited, edited
                        # A successful edit deliberately advances the public
                        # stop identity, invalidating earlier native/state
                        # authority while keeping this actual wait paused.
                        edited_thread, edited_stop = stopped(console, before)
                        assert edited_thread == thread and edited_stop > wait_stop, (edited_thread, edited_stop, wait_stop)
                        wait_stop = edited_stop
                        operands(console, thread, before_values)
                        assert b"registered" in console.send(f"break 0 0 {after}")
                        # The original monotonic deadline elapses during the real
                        # pause. Completion still requires actual manager resume.
                        time.sleep(1.1)
                        assert stopped(console, before) == (thread, wait_stop)
                        console.send("continue")
                        assert stopped(console, after)[0] == thread
                        row["returned_operands"] = operands(console, thread, [("i64", 1234), ("i32", 2)])
                        console.send("delete 2")
                        console.send("continue")
                        until = time.monotonic() + 15
                        while True:
                            status = console.send("status")
                            if b"guest exited:" in status:
                                assert b"guest exited: 0" in status, status
                                break
                            assert time.monotonic() < until, status
                            time.sleep(.01)
                    row["execution_passed"] = True
                except Exception as error:
                    row.update(execution_passed=False, error=repr(error))
                finally:
                    try:
                        if console is not None:
                            console.close()
                            row["exit_code"] = console.child.returncode
                            assert b"managed shutdown complete:" in console.transcript and b"Runtime crash" not in console.transcript, console.transcript[-4000:]
                        row["managed_quit_passed"] = console is not None
                    except Exception as error:
                        row.update(managed_quit_passed=False, close_error=repr(error))
                row["passed"] = bool(row.get("execution_passed") and row.get("managed_quit_passed"))
                rows.append(row)
                (args.out / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
                print(json.dumps({key: value for key, value in row.items() if not key.endswith("operands")}), flush=True)
    inputs = dict(binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(),
                  script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), runner_prefix=prefix)
    if prefix:
        inputs["runner_sha256"] = hashlib.sha256(Path(prefix[0]).read_bytes()).hexdigest()
    (args.out / "inputs.json").write_text(json.dumps(inputs, indent=2) + "\n")
    raise SystemExit(0 if rows and all(row["passed"] for row in rows) else 1)


if __name__ == "__main__":
    main()

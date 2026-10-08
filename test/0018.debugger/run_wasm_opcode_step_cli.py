#!/usr/bin/env python3
"""Actual Wasm opcode into/over/out regression; run only in the Linux test cgroup."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import select
import selectors
import signal
import subprocess
import time


def uleb(data, at):
    result = 0
    for shift in range(0, 35, 7):
        byte = data[at]; at += 1
        result |= (byte & 127) << shift
        if byte < 128:
            return result, at
    raise ValueError("fixture LEB overflow")


def fixture_layout(path):
    data = path.read_bytes(); assert data[:8] == b"\0asm\1\0\0\0"
    at = 8; exports = {}; bodies = []
    while at < len(data):
        kind = data[at]; size, at = uleb(data, at + 1); end = at + size
        assert end <= len(data)
        if kind == 7:
            count, at = uleb(data, at)
            for _ in range(count):
                length, at = uleb(data, at); name = data[at:at + length].decode(); at += length
                export_kind = data[at]; index, at = uleb(data, at + 1)
                if export_kind == 0: exports[name] = index
        elif kind == 10:
            count, at = uleb(data, at)
            for _ in range(count):
                length, at = uleb(data, at); bodies.append(data[at:at + length]); at += length
        at = end
    body = bodies[exports["_start"]]
    groups, at = uleb(body, 0); assert groups == 0
    expression = body[at:]; at = 0; calls = []
    while at < len(expression):
        start = at; opcode = expression[at]; at += 1
        if opcode == 0x10:
            function, at = uleb(expression, at); calls.append((start, at, function))
        elif opcode == 0x41:
            _, at = uleb(expression, at)  # fixture uses only nonnegative i32 constants
        else:
            assert opcode in (0x1a, 0x0b), opcode
    assert len(calls) == 7
    return exports, calls


class Console:
    def __init__(self, argv, log):
        self.transcript = bytearray(); self.pending = bytearray(); self.log = log
        self.child = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                      stderr=subprocess.STDOUT, preexec_fn=lambda: time.sleep(.15))
        self.selector = selectors.DefaultSelector(); self.selector.register(self.child.stdout, selectors.EVENT_READ)
        self.pidfd = os.pidfd_open(self.child.pid)
        self.retirement_proof = None
        try:
            process = Path('/proc') / str(self.child.pid)
            stat = process.joinpath('stat').read_text(); fields = stat[stat.rfind(')') + 2:].split()
            status = process.joinpath('status').read_text().splitlines()
            uid = [int(v) for v in next(line for line in status if line.startswith('Uid:')).split()[1:]]
            cgroup = process.joinpath('cgroup').read_text()
            assert int(fields[1]) == os.getpid() and uid == [os.getuid()] * 4
            assert cgroup == Path('/proc/self/cgroup').read_text()
            affinity = sorted(os.sched_getaffinity(self.child.pid))
            assert set(affinity) <= os.sched_getaffinity(0)
            executable = os.readlink(process / 'exe')
            with process.joinpath('exe').open('rb') as image:
                executable_sha256 = hashlib.file_digest(image, 'sha256').hexdigest()
            again = process.joinpath('stat').read_text()
            assert int(again[again.rfind(')') + 2:].split()[19]) == int(fields[19])
            poller = select.poll(); poller.register(self.pidfd, select.POLLIN)
            assert not poller.poll(0), 'managed VM exited before command admission'
            self.admission_proof = dict(pid=self.child.pid, birth=int(fields[19]), parent_pid=os.getpid(),
                uid=uid, cgroup=cgroup, affinity=affinity, executable=executable,
                executable_sha256=executable_sha256, pidfd_acquired_before_commands=True)
        except BaseException:
            poller = select.poll(); poller.register(self.pidfd, select.POLLIN)
            if not poller.poll(0):
                signal.pidfd_send_signal(self.pidfd, signal.SIGKILL)
            self.child.wait(timeout=5)
            os.close(self.pidfd); self.pidfd = None
            self.transcript.extend(self.child.stdout.read())
            self.selector.close(); self.log.write_bytes(self.transcript)
            raise

    def record_retirement(self):
        if self.retirement_proof is not None:
            return self.retirement_proof
        assert self.child.returncode is not None, 'managed VM has not actually been waited/reaped'
        poller = select.poll(); poller.register(self.pidfd, select.POLLIN)
        events = poller.poll(0)
        assert events and all(event & select.POLLIN and not event & select.POLLNVAL for _, event in events)
        self.retirement_proof = dict(admission=self.admission_proof, pidfd_readable=True,
                                     actual_reaped_returncode=self.child.returncode)
        self.transcript.extend(b'\n# managed-vm-retirement ' + json.dumps(self.retirement_proof).encode() + b'\n')
        os.close(self.pidfd); self.pidfd = None
        return self.retirement_proof

    def prompt(self):
        marker = b"(uwvm-debug) "; deadline = time.monotonic() + 30
        while marker not in self.pending:
            assert self.selector.select(max(0, deadline - time.monotonic())), self.transcript[-4000:]
            chunk = os.read(self.child.stdout.fileno(), 65536)
            assert chunk, (self.child.poll(), self.transcript[-4000:])
            self.pending.extend(chunk); self.transcript.extend(chunk)
        end = self.pending.index(marker) + len(marker)
        reply = bytes(self.pending[:end]); del self.pending[:end]
        return reply

    def send(self, command):
        self.transcript.extend(b"\n>>> " + command.encode() + b"\n")
        self.child.stdin.write(command.encode() + b"\n"); self.child.stdin.flush()
        reply = self.prompt(); assert not reply.startswith(b"error:"), (command, reply)
        return reply

    def location(self):
        reply = self.send("status")
        assert b"stopped:" in reply, reply
        match = re.search(rb"thread ([0-9]+) module=0 function=([0-9]+) byte-offset=([0-9]+) generation=", reply)
        assert match, reply
        return tuple(map(int, match.groups()))

    def close(self):
        try:
            if self.child.poll() is None:
                self.child.stdin.write(b"quit\n"); self.child.stdin.flush()
                self.child.stdin.close()
                try:
                    self.child.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    signal.pidfd_send_signal(self.pidfd, signal.SIGKILL); self.child.wait(timeout=5)
                    raise AssertionError("managed debug quit did not retire within 30 seconds")
            self.transcript.extend(self.child.stdout.read())
            assert self.child.returncode == 0, ("managed debug quit", self.child.returncode, self.transcript[-4000:])
        finally:
            # Retain abort/timeout output even when close itself is the failure.
            if self.child.poll() is not None:
                self.transcript.extend(self.child.stdout.read())
                self.record_retirement()
            self.selector.close(); self.log.write_bytes(self.transcript)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source-root", type=Path, required=True)
    p.add_argument("--binary", type=Path, required=True)
    p.add_argument("--wasm-tools", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args(); root = a.source_root.resolve(); a.out.mkdir(parents=True, exist_ok=False)
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    wat = root / "test/0018.debugger/fixtures/debug_wasm_opcode_step.wat"; wasm = a.out / "fixture.wasm"
    subprocess.run([str(a.wasm_tools), "parse", str(wat), "-o", str(wasm)], check=True)
    subprocess.run([str(a.wasm_tools), "validate", "--features", "all", str(wasm)], check=True)
    exports, calls = fixture_layout(wasm); rows = []
    for stack in ("instruction", "unwind"):
        for operation in ("over", "into-out", "breakpoint", "catchpoint"):
            console = None; row = {"stack": stack, "operation": operation, "actual_VM": True}
            try:
                argv = [str(a.binary), "-Rdbg", "-Rct", "0", "-Rllvm-cache-path", "disable",
                        "-Rllvm-call-stack", stack, "-WFE-tail-call", "-WFE-function-references",
                        "-WFE-reference-types", "-WFE-exceptions", "--run", str(wasm)]
                console = Console(argv, a.out / (stack + "-" + operation + ".log")); console.prompt()
                assert b"prepared; no Wasm instruction executed" in console.send("status")
                assert b"registered" in console.send(f"break 0 {exports['_start']} 0")
                console.send("continue")
                until = time.monotonic() + 20
                while b"stopped:" not in console.send("status"):
                    assert time.monotonic() < until
                    time.sleep(.01)
                thread, function, offset = console.location()
                assert (function, offset) == (exports["_start"], 0)
                console.send("delete 1")
                if operation in ("over", "into-out"):
                    for call, after, target in calls:
                        for _ in range(4):
                            _, function, offset = console.location()
                            if (function, offset) == (exports["_start"], call): break
                            console.send(f"step wasm {thread}")
                        assert (function, offset) == (exports["_start"], call)
                        if operation == "over": console.send(f"step wasm {thread} over")
                        else:
                            console.send(f"step wasm {thread}")
                            assert console.location()[1] == target
                            console.send(f"step wasm {thread} out")
                        assert console.location()[1:] == (exports["_start"], after)
                    console.send("continue")
                elif operation == "breakpoint":
                    console.send(f"break 0 {exports['leaf']} 0")
                    answer = console.send(f"step wasm {thread} over")
                    assert b"breakpoint" in answer and console.location()[1] == exports["leaf"], answer
                    console.send("delete 2"); console.send(f"step wasm {thread} out")
                    assert console.location()[1:] == (exports["_start"], calls[0][1])
                    console.send("continue")
                else:
                    console.send("catch wasm throw 0 all")
                    for call, after, target in calls:
                        for _ in range(4):
                            _, function, offset = console.location()
                            if (function, offset) == (exports["_start"], call): break
                            console.send(f"step wasm {thread}")
                        answer = console.send(f"step wasm {thread} over")
                        if target == exports["catcher"]:
                            assert b"Wasm catchpoint" in answer or b"wasm catchpoint" in answer, answer
                            break
                    console.send("disable wasm-event 1"); console.send("continue")
                until = time.monotonic() + 20
                while True:
                    answer = console.send("status")
                    if b"guest exited:" in answer:
                        assert b"guest exited: 0" in answer, answer
                        break
                    assert time.monotonic() < until, answer
                    time.sleep(.01)
                row["passed"] = True
            except Exception as error:
                row.update(passed=False, error=repr(error))
            finally:
                if console is not None: console.close()
            rows.append(row); print(json.dumps(row), flush=True)
            (a.out / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
    (a.out / "inputs.json").write_text(json.dumps({"binary_sha256": hashlib.sha256(a.binary.read_bytes()).hexdigest(),
        "fixture_sha256": hashlib.sha256(wat.read_bytes()).hexdigest(), "DWARF_required": False}, indent=2) + "\n")
    raise SystemExit(0 if all(row["passed"] for row in rows) else 1)


if __name__ == "__main__": main()

#!/usr/bin/env python3
"""Attach the real stdio DAP adapter to a live, host-authorized Unix VM."""

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import selectors
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "0017.runtime"))
from run_llvm_debug_server_macos import MARKER, escape, wait_for


def send_dap(process, sequence, command, arguments=None):
    body = json.dumps({"seq": sequence, "type": "request", "command": command,
                       "arguments": arguments or {}}, separators=(",", ":")).encode()
    os.write(process.stdin.fileno(), b"Content-Length: " + str(len(body)).encode()
             + b"\r\n\r\n" + body)


def read_dap(process, sequence, pending, transcript, event_name=None):
    deadline = time.monotonic() + 20
    with selectors.DefaultSelector() as selector:
        selector.register(process.stdout, selectors.EVENT_READ)
        while time.monotonic() < deadline:
            while b"\r\n\r\n" in pending:
                header, _, _ = pending.partition(b"\r\n\r\n")
                assert header.startswith(b"Content-Length: "), header
                length = int(header[16:])
                prefix = len(header) + 4
                if len(pending) < prefix + length:
                    break
                message = json.loads(pending[prefix:prefix + length])
                del pending[:prefix + length]
                transcript.append(message)
                if message.get("type") == "response" and message.get("request_seq") == sequence:
                    return message
                if event_name is not None and message.get("type") == "event" and message.get("event") == event_name:
                    return message
            if selector.select(max(0, deadline - time.monotonic())):
                chunk = os.read(process.stdout.fileno(), 65536)
                assert chunk, (process.poll(), transcript)
                pending.extend(chunk)
        raise TimeoutError((sequence, transcript[-8:]))


def default_rustc():
    if shutil.which("rustup"):
        probe = subprocess.run(["rustup", "which", "rustc"], capture_output=True, text=True)
        if probe.returncode == 0:
            return probe.stdout.strip()
    return shutil.which("rustc")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", required=True, type=Path)
    parser.add_argument("--wasm-tools", type=Path, default=shutil.which("wasm-tools"))
    parser.add_argument("--out", type=Path)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--source-step", action="store_true")
    parser.add_argument("--source-language", choices=("c", "cpp", "rust"), default="c")
    parser.add_argument("--clang", default="clang")
    parser.add_argument("--rustc", default=default_rustc())
    args = parser.parse_args()
    if sys.platform not in ("darwin", "linux") or not args.wasm_tools:
        parser.error("macOS or Linux and wasm-tools are required")
    product = args.uwvm.resolve(strict=True)
    root = Path(__file__).resolve().parents[2]
    broker_script = root / ("tools/debug/secure_server_macos.py" if sys.platform == "darwin"
                            else "tools/debug/secure_server.py")
    adapter_script = root / "tools/debug/dap_adapter.py"
    with tempfile.TemporaryDirectory(prefix="uwvm-dap-macos-") as directory:
        work = Path(directory)
        private = work / "private"
        private.mkdir(mode=0o700)
        wasm = work / "loop.wasm"
        if args.source_step:
            suffix = {"c": ".c", "cpp": ".cc", "rust": ".rs"}[args.source_language]
            source = work / ("loop" + suffix)
            if args.source_language == "rust":
                source.write_text('''#![no_std]
#![crate_type = "cdylib"]
static mut COUNTER: u32 = 0;
#[no_mangle]
pub extern "C" fn _start() {
  let mut value = 0u32;
  loop {
    value += 1;
    unsafe { COUNTER = value; }
  }
}
#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! { loop {} }
''')
                subprocess.run([args.rustc, "--target", "wasm32-unknown-unknown",
                                "-C", "panic=abort", "-C", "debuginfo=2", "-C", "opt-level=0",
                                "-C", "overflow-checks=off",
                                str(source), "-o", str(wasm)], check=True)
            else:
                entry_prefix = 'extern "C" ' if args.source_language == "cpp" else ""
                source.write_text('volatile unsigned counter;\n' + entry_prefix + '''__attribute__((export_name("_start"))) void _start(void) {
  for (;;) {
    counter++;
    counter += 2;
  }
}\n''')
                subprocess.run([args.clang, "--target=wasm32-unknown-unknown", "-O0", "-g", "-gdwarf-4",
                                "-nostdlib", "-Wl,--no-entry", "-Wl,--export=_start",
                                str(source), "-o", str(wasm)], check=True)
        else:
            wat = work / "loop.wat"
            wat.write_text(f'''(module
          (import "wasi_snapshot_preview1" "fd_write" (func $write
            (param i32 i32 i32 i32) (result i32)))
          (memory (export "memory") 1)
          (data (i32.const 128) "{escape(MARKER)}")
          (func (export "_start")
            i32.const 0 i32.const 128 i32.store
            i32.const 4 i32.const {len(MARKER)} i32.store
            i32.const 1 i32.const 0 i32.const 1 i32.const 100 call $write drop
            (loop $spin i32.const 1 drop br $spin)))\n''')
            subprocess.run([str(args.wasm_tools), "parse", str(wat), "-o", str(wasm)], check=True)
        subprocess.run([str(args.wasm_tools), "validate", str(wasm)], check=True)
        vm_args = ["-m", "run", "-Rct", "0", "-Rllvm-call-stack", "unwind",
                   "-Rllvm-cache-path", "disable"]
        if not args.ros:
            vm_args += ["-Rcc", "jit", "-Rcm", "full"]
        vm_args += ["--run", str(wasm)]
        broker = subprocess.Popen([sys.executable, str(broker_script), "serve", "--uwvm", str(product),
                                   "--socket-dir", str(private), "--", *vm_args],
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0)
        broker_log = bytearray()
        adapter = None
        guest_pid = None
        try:
            startup_marker = (b"guest pid: " if args.source_step else MARKER) if sys.platform == "darwin" \
                else b"debug server: "
            wait_for(broker.stdout, startup_marker, broker_log)
            for line in broker_log.splitlines():
                if line.startswith(b"guest pid: "):
                    guest_pid = int(line.partition(b": ")[2])
            if sys.platform == "darwin":
                assert guest_pid is not None, broker_log
            adapter = subprocess.Popen([sys.executable, str(adapter_script)],
                                       stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, bufsize=0)
            pending = bytearray()
            transcript = []
            def request(sequence, command, arguments=None):
                send_dap(adapter, sequence, command, arguments)
                response = read_dap(adapter, sequence, pending, transcript)
                assert response["success"], (command, response, transcript)
                return response
            request(1, "initialize", {"adapterID": "uwvm-llvm-full"})
            request(2, "attach", {"socketDir": str(private), "moduleId": 0, "stepLevel": "wasm"})
            request(3, "configurationDone")
            request(4, "pause", {"threadId": 1})
            threads = request(5, "threads")["body"]["threads"]
            assert threads and threads[0]["id"] == 1, threads
            request(6, "stepIn", {"threadId": 1, "granularity": "statement"})
            next_sequence = 7
            if args.source_step:
                source_step = "next" if args.source_language == "rust" else "stepIn"
                request(next_sequence, source_step, {"threadId": 1, "granularity": "line"})
                read_dap(adapter, None, pending, transcript, event_name="stopped")
                next_sequence += 1
                frames = request(next_sequence, "stackTrace", {"threadId": 1})["body"]["stackFrames"]
                assert frames and frames[0].get("source", {}).get("path", "").endswith(source.name), frames
                next_sequence += 1
                instruction = frames[0]["instructionPointerReference"]
                points = request(next_sequence, "setInstructionBreakpoints", {
                    "breakpoints": [{"instructionReference": instruction}],
                })["body"]["breakpoints"]
                assert len(points) == 1 and points[0]["verified"], points
                next_sequence += 1
                request(next_sequence, "setInstructionBreakpoints", {"breakpoints": []})
                next_sequence += 1
                source_path = frames[0]["source"]["path"]
                points = request(next_sequence, "setBreakpoints", {
                    "source": {"path": source_path},
                    "breakpoints": [{"line": frames[0]["line"]}],
                })["body"]["breakpoints"]
                assert len(points) == 1 and points[0]["verified"], points
                next_sequence += 1
                request(next_sequence, "setBreakpoints", {
                    "source": {"path": source_path}, "breakpoints": [],
                })
                next_sequence += 1
                memory = request(next_sequence, "readMemory", {
                    "memoryReference": "wasm-memory:0:0:0", "count": 4,
                })["body"]
                assert len(base64.b64decode(memory["data"])) == 4, memory
                next_sequence += 1
            request(next_sequence, "stepIn", {"threadId": 1, "granularity": "instruction"})
            request(next_sequence + 1, "disconnect")
            assert any(event.get("event") == "stopped" for event in transcript), transcript
            assert broker.poll() is None
            if args.out:
                args.out.mkdir(parents=True, exist_ok=True)
                (args.out / "dap-transcript.json").write_text(json.dumps({
                    "product_sha256": hashlib.sha256(product.read_bytes()).hexdigest(),
                    "wasm_sha256": hashlib.sha256(wasm.read_bytes()).hexdigest(),
                    "events": transcript,
                }, indent=2) + "\n")
            print("PASS " + ("macOS" if sys.platform == "darwin" else "Linux")
                  + " live DAP attach, pause, Wasm"
                  + ("/" + args.source_language + " source" if args.source_step else "")
                  + "/native step and detach")
        finally:
            if adapter is not None:
                if adapter.poll() is None:
                    adapter.terminate()
                adapter.wait(timeout=10)
            if broker.poll() is None:
                broker.terminate()
            broker.wait(timeout=15)
            if guest_pid is not None:
                try:
                    os.kill(guest_pid, 15)
                except ProcessLookupError:
                    pass
            broker_log.extend(broker.stdout.read())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Linux regression: the first real host command arrives before VM adoption.

The host-only launch gate waits for POLLIN without reading the command, then
execs the unchanged full LLVM JIT product with its original inherited FD.
Production broker authentication and VM credential checks remain in force.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import select
import sys
import run_dap_current_broker as live
from run_dap_frame_scope_lifetime import Dispatch, ScheduledBroker


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module


def main():
    root = Path(__file__).resolve().parents[2]
    if len(sys.argv) > 1 and sys.argv[1] == "__vm_gate":
        command = sys.argv[2:]
        descriptor = int(command[command.index("--debug-jit-control-fd") + 1])
        pending = select.poll(); pending.register(descriptor, select.POLLIN)
        live.require(any(events & select.POLLIN for _, events in pending.poll(8000)), "first host command did not reach launch gate")
        print("queued-start gate: first host command pending before VM exec", flush=True)
        os.execv(command[0], command)
    if len(sys.argv) > 1 and sys.argv[1] == "serve":
        server = load(root / "tools/debug/secure_server.py", "queued_actual_server")
        original = server.subprocess.Popen
        def launch(command, *args, **kwargs):
            return original([sys.executable, str(Path(__file__).resolve()), "__vm_gate", *command], *args, **kwargs)
        server.subprocess.Popen = launch
        return server.main()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", required=True, type=Path)
    parser.add_argument("--wasm", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--ros", action="store_true")
    args = parser.parse_args()
    live.require(sys.platform == "linux", "Linux credential regression required")
    args.out.mkdir(mode=0o700)
    dap = load(root / "tools/debug/dap_adapter.py", "queued_actual_adapter")
    paths = (args.uwvm, args.wasm, Path(__file__).resolve(), root / "tools/debug/secure_server.py",
             root / "tools/debug/dap_adapter.py", Path(live.__file__), root / "test/0018.debugger/run_dap_frame_scope_lifetime.py")
    pins = {str(path): live.sha(path) for path in paths}
    mode = ["-Raot"] if args.ros else ["-Rcc", "jit", "-Rcm", "full"]
    vm = ["-m", "run", *mode, "-Rct", "0", "-Rllvm-call-stack", "instruction", "-Rllvm-cache-path", "disable", "--run", str(args.wasm)]
    server = live.BrokerSession(args.uwvm, Path(__file__).resolve(), root / "tools/debug/dap_adapter.py", vm, args.out)
    broker = None; record = {"passed": False, "pins": pins}
    try:
        broker = ScheduledBroker(dap.UnixBroker(str(server.directory))); client = Dispatch(dap, broker)
        first = client.evaluate("status")
        live.require(b"guest exited:" not in first and b"error:" not in first, "first queued actual command refused", first)
        client.evaluate("pause")
        current = live.actual_location(client)
        client.evaluate(f"set wasm global 0 0 {current['thread']} bits i32 0")
        result = client.evaluate("continue")
        for _ in range(20):
            if b"guest exited: 0" in result: break
            result = client.evaluate("wait")
        live.require(b"guest exited: 0" in result, "queued-start guest natural exit", result)
        record.update(passed=True, first_reply=first.decode(), actual_guest_exit=result.decode())
    finally:
        if "client" in locals(): record["requests"] = client.requests
        if broker is not None: record["broker_commands"] = broker.commands; broker.close()
        try:
            if record["passed"]:
                try: server.child.wait(timeout=10)
                except BaseException as error:
                    record.update(passed=False, natural_exit_wait_error=repr(error)); raise
        finally:
            try: record["cleanup"] = server.close()
            finally:
                record["pins_after"] = {n: live.sha(Path(n)) for n in pins}
                live.require(record["pins_after"] == pins, "queued-start inputs changed")
                raw = (args.out / "broker.raw").read_text()
                record["queued_before_exec"] = "queued-start gate: first host command pending before VM exec" in raw
                live.require(record["queued_before_exec"], "actual launch gate evidence missing", raw)
                (args.out / "queued-start.json").write_text(json.dumps(record, indent=2) + "\n")
    print("run_dap_broker_queued_start: PASS first-command-before-exec natural-exit=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

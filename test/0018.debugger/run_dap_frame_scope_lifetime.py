#!/usr/bin/env python3
"""Actual Linux JIT/broker lifetime checks with deterministic external control.

Adapter.handle writes real DAP framing in this process, without idle polling.
Every status, local value and external step comes from the real owned VM. This
is request-dispatch acceptance, not a second stdio or IDE acceptance claim.
"""
import argparse
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import sys
import run_dap_current_broker as live


class Dispatch:
    def __init__(self, module, broker):
        self.output = io.BytesIO()
        self.adapter = module.Adapter(self.output)
        self.adapter.broker = broker
        self.sequence = 0
        self.requests = []
        self.events = []

    def request(self, command, arguments=None, success=True):
        self.sequence += 1
        request = {"type": "request", "seq": self.sequence, "command": command, "arguments": arguments or {}}
        self.adapter.handle(request)
        stream = io.BytesIO(self.output.getvalue())
        self.output.seek(0); self.output.truncate()
        rows = []
        while prefix := stream.readline():
            live.require(re.fullmatch(rb"Content-Length: [0-9]+\r\n", prefix) and stream.readline() == b"\r\n", "actual DAP response framing")
            rows.append(json.loads(stream.read(int(prefix[16:-2]))))
        replies = [row for row in rows if row["type"] == "response"]
        live.require(len(replies) == 1 and replies[0]["request_seq"] == self.sequence and replies[0]["command"] == command,
                     "exactly one response for actual request", rows)
        self.events.extend(row for row in rows if row["type"] == "event")
        self.requests.append({"request": request, "messages": rows})
        if success is not None:
            live.require(replies[0]["success"] is success, "actual DAP request outcome", replies[0])
        return replies[0]

    def evaluate(self, expression):
        return self.request("evaluate", {"context": "repl", "expression": expression})["body"]["result"].encode()


class ScheduledBroker:
    def __init__(self, channel):
        self.channel = channel
        self.commands = []
        self.after_locals = None

    def request(self, command):
        result = self.channel.request(command)
        self.commands.append({"command": command, "reply": result})
        if command.startswith("locals ") and self.after_locals is not None:
            hook, self.after_locals = self.after_locals, None
            hook()
        return result

    def close(self):
        self.channel.close()


def handles(client, thread):
    frame = live.physical_frame(client, thread)
    scopes = client.request("scopes", {"frameId": frame["id"]})["body"]["scopes"]
    local = [row for row in scopes if row["name"] == "Wasm locals"]
    live.require(len(local) == 1 and frame["source"]["sourceReference"] == frame["id"], "actual Wasm labels and scope", (frame, scopes))
    values = client.request("variables", {"variablesReference": local[0]["variablesReference"]})["body"]["variables"]
    live.require(len(values) == 1 and values[0]["value"] == "i32=17" and "memoryReference" not in values[0], "actual current local marker", values)
    return frame["id"], local[0]["variablesReference"], frame["source"]["sourceReference"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", required=True, type=Path)
    parser.add_argument("--wasm", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--policy", required=True, choices=("instruction", "unwind"))
    parser.add_argument("--ros", action="store_true")
    args = parser.parse_args()
    live.require(sys.platform == "linux" and os.uname().machine == "x86_64", "Linux x86_64 required")
    args.out.mkdir(mode=0o700)
    root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location("actual_lifetime_dap", root / "tools/debug/dap_adapter.py")
    dap = importlib.util.module_from_spec(spec); spec.loader.exec_module(dap)
    paths = (args.uwvm, args.wasm, Path(__file__), Path(live.__file__), root / "tools/debug/dap_adapter.py",
             root / "tools/debug/secure_server.py", root / "test/0017.runtime/run_debug_source_step_cli.py",
             root / "test/0017.runtime/run_debug_source_inline_metadata_cli.py")
    pins = {str(path): live.sha(path) for path in paths}
    function, body = live.source_cli.metadata_cli.function(args.wasm, "spin")
    groups, offset = live.source_cli.metadata_cli.u32(body, 0, len(body))
    live.require(groups == 1 and body[offset:offset + 2] == b"\x01\x7f", "actual fixture has exactly one i32 local", body)
    offset += 2
    live.require(body[offset:offset + 5] == b"\x41\x11\x21\x00\x03", "actual local marker initialization", body)
    mode = ["-Raot"] if args.ros else ["-Rcc", "jit", "-Rcm", "full"]
    vm = ["-m", "run", *mode, "-Rct", "0", "-Rllvm-call-stack", args.policy,
          "-Rllvm-exception-dispatch", "native-unwind", "-Rllvm-cache-path", "disable", "--run", str(args.wasm)]
    server = live.BrokerSession(args.uwvm, root / "tools/debug/secure_server.py", root / "tools/debug/dap_adapter.py", vm, args.out)
    broker = None
    record = {"passed": False, "pins": pins, "cases": [], "scope": __doc__}
    try:
        broker = ScheduledBroker(dap.UnixBroker(str(server.directory)))
        client = Dispatch(dap, broker)
        current, point = live.breakpoint_begin(client, function, 6)
        client.evaluate(f"delete {point}")
        thread = current["thread"]
        for command, index, key in (("scopes", 0, "frameId"), ("variables", 1, "variablesReference"), ("source", 2, "sourceReference")):
            old = handles(client, thread)
            before = live.actual_location(client)
            # Bypass only adapter state bookkeeping. The channel still sends a
            # genuine authenticated external Wasm step to the actual controller.
            external = broker.request(f"step wasm {thread}")
            state, _, rows, _ = dap.parse_status(external)
            live.require(state == "stopped" and rows[0]["stop_id"] > before["stop"] and "native_pc" not in rows[0], "actual external Wasm stop", external)
            begin = len(broker.commands)
            stale = client.request(command, {key: old[index]}, success=False)
            live.require(not any(row["command"].startswith("locals ") for row in broker.commands[begin:]), "stale scope must refuse before actual locals query")
            fresh = handles(client, thread)
            live.require(fresh[0] > old[0], "actual fresh session-unique frame")
            client.request(command, {key: fresh[index]})
            record["cases"].append({"route": command, "before": before, "external": external, "old": old, "fresh": fresh, "refusal": stale})
        _, scope, _ = handles(client, thread)
        before = live.actual_location(client)
        broker.after_locals = lambda: broker.request(f"step wasm {thread}")
        stale = client.request("variables", {"variablesReference": scope}, success=False)
        after = live.actual_location(client)
        live.require(after["stop"] > before["stop"] and "body" not in stale, "actual mid-copy change must withhold local data", stale)
        handles(client, thread)
        record["cases"].append({"route": "variables-mid-copy", "before": before, "after": after, "refusal": stale})
        client.evaluate(f"set wasm global 0 0 {thread} bits i32 0")
        result = client.evaluate("continue")
        for _ in range(20):
            if b"guest exited: 0" in result: break
            result = client.evaluate("wait")
        live.require(b"guest exited: 0" in result, "actual guest natural exit", result)
        live.require(any(row.get("event") == "exited" and row["body"]["exitCode"] == 0 for row in client.events), "actual DAP exit event")
        record.update({"passed": True, "requests": client.requests, "broker_commands": broker.commands, "actual_guest_exit": result.decode()})
    finally:
        if broker is not None:
            record["broker_commands"] = broker.commands
        if "client" in locals():
            record["requests"] = client.requests
        if broker is not None: broker.close()
        try:
            # In-process dispatch has no adapter child whose EOF retirement
            # gives the real broker time to reap its guest. Wait for that own
            # natural exit before the generic failure-cleanup path can signal.
            if record["passed"]:
                try:
                    server.child.wait(timeout=10)
                except BaseException as error:
                    record["passed"] = False
                    record["natural_exit_wait_error"] = repr(error)
                    raise
        finally:
            try:
                record["cleanup"] = server.close()
            finally:
                record["pins_after"] = {name: live.sha(Path(name)) for name in pins}
                live.require(record["pins_after"] == pins, "immutable actual lifetime inputs changed")
                (args.out / "frame-scope-lifetime.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"run_dap_frame_scope_lifetime: PASS policy={args.policy} actual-external-stop=3 actual-mid-copy=1 natural-exit=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

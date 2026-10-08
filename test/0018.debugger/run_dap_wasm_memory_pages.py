#!/usr/bin/env python3
"""Actual guest memory32/memory64 pages, fresh stops, and native-trap refusal.

Uses real DAP request dispatch and the authenticated broker. No guest replies
are fabricated. This is Linux native acceptance, not an IDE acceptance claim.
"""
import argparse
import base64
import importlib.util
import json
import os
from pathlib import Path
import sys
import time
import run_dap_current_broker as live
from run_dap_frame_scope_lifetime import Dispatch, ScheduledBroker
from run_dap_wasm_scope_pages import recurring_stop


class PageBroker(ScheduledBroker):
    def __init__(self, channel):
        super().__init__(channel)
        self.after_page = None

    def request(self, command):
        result = super().request(command)
        if command.startswith("memory ") and self.after_page is not None:
            self.after_page()
        return result


def arguments(memory, address, count, offset=0):
    return {"memoryReference": f"wasm-memory:0:{memory}:{address}", "offset": offset, "count": count}


def check_read(client, broker, memory, address, count, offset=0):
    first = len(broker.commands)
    reply = client.request("readMemory", arguments(memory, address, count, offset))
    body = reply["body"]
    data = base64.b64decode(body["data"], validate=True)
    live.require(body["address"] == str(address + offset) and body["unreadableBytes"] == 0
                 and data == bytes([81 if memory == 0 else 167]) * count,
                 "actual exact guest bytes and numeric DAP address", (memory, address, count, body))
    copies = [row["command"] for row in broker.commands[first:] if row["command"].startswith("memory ")]
    expected = [f"memory 0 {memory} {address+offset+i} {min(256, count-i)}" for i in range(0, max(1, count), 256)]
    live.require(copies == expected, "actual contiguous bounded guest memory copies", copies)
    return {"memory": memory, "address": address, "offset": offset, "count": count,
            "copies": len(copies), "response": reply}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", required=True, type=Path)
    parser.add_argument("--wasm", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--policy", required=True, choices=("instruction", "unwind"))
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--stop-baseline", action="store_true")
    args = parser.parse_args()
    live.require(sys.platform == "linux" and os.uname().machine == "x86_64", "Linux x86_64 required")
    args.out.mkdir(mode=0o700)
    root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location("actual_memory_pages_dap", root / "tools/debug/dap_adapter.py")
    dap = importlib.util.module_from_spec(spec); spec.loader.exec_module(dap)
    paths = (args.uwvm, args.wasm, Path(__file__), Path(live.__file__), root / "tools/debug/dap_adapter.py",
             root / "tools/debug/secure_server.py", root / "test/0018.debugger/run_dap_frame_scope_lifetime.py",
             root / "test/0018.debugger/run_dap_wasm_scope_pages.py",
             root / "test/0017.runtime/run_debug_source_step_cli.py",
             root / "test/0017.runtime/run_debug_source_inline_metadata_cli.py")
    pins = {str(p): live.sha(p) for p in paths}
    function, body = live.source_cli.metadata_cli.function(args.wasm, "spin")
    groups, cursor = live.source_cli.metadata_cli.u32(body, 0, len(body))
    live.require(groups == 1 and body[cursor:cursor+2] == b"\x01\x7f", "actual one i32 local declaration")
    cursor += 2
    loop = body.find(b"\x03\x40\x23\x01", cursor)
    live.require(loop > cursor and body.find(b"\x03\x40\x23\x01", loop+1) == -1, "unique recurring memory fixture safepoint")
    target = loop + 2 - cursor
    mode = ["-Raot"] if args.ros else ["-Rcc", "jit", "-Rcm", "full"]
    vm = ["--wasm-feature-enable-memory64", "--wasm-feature-enable-multi-memory", "--wasm-feature-enable-bulk-memory",
          "-m", "run", *mode, "-Rct", "0", "-Rllvm-call-stack", args.policy,
          "-Rllvm-exception-dispatch", "native-unwind", "-Rllvm-cache-path", "disable", "--run", str(args.wasm)]
    server = live.BrokerSession(args.uwvm, root / "tools/debug/secure_server.py", root / "tools/debug/dap_adapter.py", vm, args.out)
    broker = None
    record = {"passed": False, "pins": pins, "reads": [], "range_refusals": [], "mid_copy": [], "startup_settle_seconds": 3}
    try:
        time.sleep(record["startup_settle_seconds"])
        live.require(server.child.poll() is None, "memory fixture exited before debugger startup", server.child.poll())
        broker = PageBroker(dap.UnixBroker(str(server.directory)))
        client = Dispatch(dap, broker); client.adapter.step_level = "wasm"
        current, point = live.breakpoint_begin(client, function, target); client.evaluate(f"delete {point}")
        thread = current["thread"]
        if not args.stop_baseline:
            for memory in (0, 1):
                for count in (0, 1, 128, 256, 257, 4096, 65536):
                    record["reads"].append(check_read(client, broker, memory, 64, count, -47))
                for address, count in ((131071, 1), (131072, 0)):
                    record["reads"].append(check_read(client, broker, memory, address, count))
                for address, count in ((131073, 0), (131071, 2), (130750, 512)):
                    first = len(broker.commands)
                    reply = client.request("readMemory", arguments(memory, address, count), success=False)
                    live.require("body" not in reply, "actual failed range published a prefix", reply)
                    record["range_refusals"].append({"memory": memory, "address": address, "count": count, "response": reply,
                        "commands": broker.commands[first:]})
            for ref in ("wasm-memory:999:0:17", "wasm-memory:0:2:17"):
                reply = client.request("readMemory", {"memoryReference": ref, "count": 4}, success=False)
                live.require("body" not in reply, "actual unavailable module/memory published bytes", reply)
                record["range_refusals"].append({"reference": ref, "response": reply})
            record["invalid"] = []
            for entry in ({"memoryReference": "wasm-memory:0:0:17", "count": 65537},
                          {"memoryReference": "wasm-memory:0:0:17", "count": True},
                          {"memoryReference": "wasm-memory:0:0:17", "count": 4, "offset": -18},
                          {"memoryReference": "wasm-memory:0:0:18446744073709551615", "count": 1},
                          {"memoryReference": "0x1234", "count": 4}):
                before = len(broker.commands)
                reply = client.request("readMemory", entry, success=False)
                live.require(len(broker.commands) == before and "body" not in reply, "invalid selector caused a native borrow", reply)
                record["invalid"].append(reply)
        # An actual commit followed by an actual step invalidates the copied
        # four bytes. The old adapter returns their stale value UVFRUQ==.
        current = live.actual_location(client); thread = current["thread"]
        stale = {"before": current}
        def advance_changed_bytes():
            broker.after_page = None
            stale["mutation"] = broker.request(f"set wasm memory 0 0 {thread} 17 bytes ffaabbcc")
            live.require(" applied=1 " in stale["mutation"] and "target=memory" in stale["mutation"], "actual guest memory commit", stale["mutation"])
            stale["external_step"] = broker.request(f"step wasm {thread}")
            raw = broker.request("status")
            stopped = dap.parse_status(raw)
            live.require(stopped[0] == "stopped" and len(stopped[2]) == 1, "actual external stop cohort", raw)
            stale["after"] = {"stop": stopped[2][0]["stop_id"], "raw": raw}
            stale["actual_memory_after"] = broker.request("memory 0 0 17 4")
            live.require(stale["after"]["stop"] > current["stop"] and stale["actual_memory_after"] == "memory: ff aa bb cc\n",
                         "actual changed guest memory and fresh stop", stale)
        broker.after_page = advance_changed_bytes
        stale["response"] = client.request("readMemory", arguments(0, 17, 4), success=None)
        record["changed_bytes"] = stale
        live.require(not stale["response"]["success"] and "body" not in stale["response"], "stale copied memory crossed an actual stop", stale)
        current = recurring_stop(client, function, target); thread = current["thread"]
        restore = client.evaluate(f"set wasm memory 0 0 {thread} 17 bytes 51515151")
        live.require(b" applied=1 " in restore, "actual fixture byte restoration", restore)
        for memory, changed_page in ((0, 1), (1, 2), (1, 3)):
            current = recurring_stop(client, function, target); thread = current["thread"]
            copied = 0; event = {"memory": memory, "changed_page": changed_page, "before": current}
            def advance():
                nonlocal copied
                copied += 1
                if copied == changed_page:
                    broker.after_page = None
                    event["external_step"] = broker.request(f"step wasm {thread}")
            broker.after_page = advance
            event["response"] = client.request("readMemory", arguments(memory, 17, 513), success=False)
            event["after"] = live.actual_location(client); event["copies"] = copied
            live.require(copied == changed_page and event["after"]["stop"] > current["stop"]
                         and "body" not in event["response"], "actual mid/final-copy prefix escaped", event)
            record["mid_copy"].append(event)
        current = recurring_stop(client, function, target); thread = current["thread"]
        record["native_step"] = client.evaluate(f"step asm {thread}").decode()
        native = dap.parse_status(record["native_step"])
        live.require(native[0] == "stopped" and native[2] and native[2][0].get("native_pc") and native[2][0].get("stop_id"),
                     "actual identified native trap", record["native_step"])
        first = len(broker.commands)
        reply = client.request("readMemory", arguments(0, 17, 4), success=False)
        live.require("body" not in reply and not any(r["command"].startswith("memory ") for r in broker.commands[first:]),
                     "native trap borrowed guest or host memory", reply)
        record["native_refusal"] = reply
        current = recurring_stop(client, function, target)
        for memory in (0, 1): record["reads"].append(check_read(client, broker, memory, 17, 257))
        client.evaluate(f"set wasm global 0 0 {current['thread']} bits i32 0")
        result = client.evaluate("continue")
        for _ in range(20):
            if b"guest exited: 0" in result: break
            result = client.evaluate("wait")
        live.require(b"guest exited: 0" in result, "actual memory fixture natural exit", result)
        record.update(passed=True, actual_guest_exit=result.decode())
    finally:
        if "client" in locals(): record["requests"] = client.requests
        if broker is not None:
            record["broker_commands"] = broker.commands; broker.after_page = None; broker.close()
        try:
            if record["passed"]:
                try: server.child.wait(timeout=10)
                except BaseException as error:
                    record.update(passed=False, natural_exit_wait_error=repr(error)); raise
        finally:
            try: record["cleanup"] = server.close()
            finally:
                record["pins_after"] = {n: live.sha(Path(n)) for n in pins}
                live.require(record["pins_after"] == pins, "immutable actual memory page inputs changed")
                (args.out / "wasm-memory-pages.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"run_dap_wasm_memory_pages: PASS policy={args.policy} memory32+memory64 copies<=256 read<=65536 stale=5 natural-exit=0")
    return 0


if __name__ == "__main__": raise SystemExit(main())

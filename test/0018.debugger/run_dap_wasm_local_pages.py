#!/usr/bin/env python3
"""Real 161-local JIT pages and GC expansion, only in the Linux keeper cgroup."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import sys
import run_dap_current_broker as live
from run_dap_frame_scope_lifetime import Dispatch, ScheduledBroker


def scope(client, thread):
    frame = live.physical_frame(client, thread)
    scopes = client.request("scopes", {"frameId": frame["id"]})["body"]["scopes"]
    return next(s["variablesReference"] for s in scopes if s["name"] == "Wasm locals")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", required=True, type=Path)
    parser.add_argument("--wasm", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--policy", required=True, choices=("instruction", "unwind"))
    parser.add_argument("--ros", action="store_true")
    args = parser.parse_args()
    live.require(sys.platform == "linux" and os.uname().machine == "x86_64", "Linux x86_64 required")
    args.out.mkdir(mode=0o700); root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location("actual_local_pages_dap", root / "tools/debug/dap_adapter.py")
    dap = importlib.util.module_from_spec(spec); spec.loader.exec_module(dap)
    paths = (args.uwvm, args.wasm, Path(__file__), Path(live.__file__), root / "tools/debug/dap_adapter.py",
        root / "tools/debug/secure_server.py", root / "test/0018.debugger/run_dap_frame_scope_lifetime.py",
        root / "test/0017.runtime/run_debug_source_step_cli.py", root / "test/0017.runtime/run_debug_source_inline_metadata_cli.py")
    pins = {str(p): live.sha(p) for p in paths}
    function, body = live.source_cli.metadata_cli.function(args.wasm, "spin")
    groups, cursor = live.source_cli.metadata_cli.u32(body, 0, len(body)); live.require(groups == 2, "actual fixture local groups")
    count, cursor = live.source_cli.metadata_cli.u32(body, cursor, len(body))
    live.require(count == 160 and body[cursor:cursor + 4] == b"\x7f\x01\x63\x00", "actual i32 and GC local declarations", body)
    cursor += 4; loop = body.find(b"\x03\x40\x23\x01", cursor)
    live.require(loop > cursor and body.find(b"\x03\x40\x23\x01", loop + 1) == -1, "unique recurring Wasm safepoint")
    target = loop + 2 - cursor
    mode = ["-Raot"] if args.ros else ["-Rcc", "jit", "-Rcm", "full"]
    vm = ["--wasm-feature-enable-gc", "--wasm-feature-enable-function-references", "--wasm-feature-enable-reference-types",
          "-m", "run", *mode, "-Rct", "0", "-Rllvm-call-stack", args.policy,
          "-Rllvm-exception-dispatch", "native-unwind", "-Rllvm-cache-path", "disable", "--run", str(args.wasm)]
    server = live.BrokerSession(args.uwvm, root / "tools/debug/secure_server.py", root / "tools/debug/dap_adapter.py", vm, args.out)
    broker = None; record = {"passed": False, "pins": pins, "pages": []}
    try:
        broker = ScheduledBroker(dap.UnixBroker(str(server.directory))); client = Dispatch(dap, broker)
        current, point = live.breakpoint_begin(client, function, target); client.evaluate(f"delete {point}")
        thread = current["thread"]; ref = scope(client, thread)
        for start, count, indices in ((129, 8, range(129, 137)), (63, 80, range(63, 143)), (158, 2, range(158, 160)), (161, 4, [])):
            result = client.request("variables", {"variablesReference": ref, "start": start, "count": count, "filter": "indexed"})
            values = result["body"]["variables"]
            live.require([v["name"] for v in values] == [f"local {i}" for i in indices] and
                         [v["value"] for v in values] == [f"i32={1000+i}" for i in indices], "actual original-index locals page", result)
            live.require(all("memoryReference" not in v for v in values), "local page host memory capability")
            record["pages"].append({"start": start, "count": count, "response": result})
        result = client.request("variables", {"variablesReference": ref, "start": 158, "count": 0})
        values = result["body"]["variables"]; live.require(len(values) == 3, "count0 actual remaining locals", values)
        gc = values[-1]["variablesReference"]; live.require(gc > 0 and values[-1]["name"] == "local 160", "actual rich local GC root", values)
        members = client.request("variables", {"variablesReference": gc})["body"]["variables"]
        live.require(len(members) == 1 and members[0]["value"] == "42" and "memoryReference" not in members[0], "actual GC field expansion", members)
        record["remaining_and_gc"] = {"response": result, "members": members}
        all_values = client.request("variables", {"variablesReference": ref})["body"]["variables"]
        live.require(len(all_values) == 161 and all_values[159]["value"] == "i32=1159", "default actual full locals beyond diagnostic prefix")
        result = client.request("variables", {"variablesReference": ref, "filter": "named"})
        live.require(result["body"]["variables"] == [], "indexed local filter"); record["named"] = result
        external = broker.request(f"step wasm {thread}")
        stale = client.request("variables", {"variablesReference": ref, "start": 129, "count": 8}, success=False)
        stale_gc = client.request("variables", {"variablesReference": gc}, success=False)
        record["stale_after_external"] = {"step": external, "scope": stale, "gc": stale_gc}
        ref = scope(client, thread)
        fresh = client.request("variables", {"variablesReference": ref, "start": 129, "count": 1})
        live.require(fresh["body"]["variables"][0]["value"] == "i32=1129", "fresh page after external stop")
        client.evaluate(f"set wasm global 0 0 {thread} bits i32 0"); result = client.evaluate("continue")
        for _ in range(20):
            if b"guest exited: 0" in result: break
            result = client.evaluate("wait")
        live.require(b"guest exited: 0" in result, "actual natural guest logical exit", result)
        record.update(passed=True, actual_guest_exit=result.decode(), total_locals=161)
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
                live.require(record["pins_after"] == pins, "immutable actual local-page inputs changed")
                (args.out / "wasm-local-pages.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"run_dap_wasm_local_pages: PASS policy={args.policy} actual-locals=161 GC-member=42 natural-exit=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

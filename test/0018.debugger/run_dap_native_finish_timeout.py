#!/usr/bin/env python3
"""Real nonreturning native stepOut timeout through broker/DAP, keeper only.

The guest has a proved Wasm parent, but its callee loops until a real Wasm global
is changed. A timeout may preserve a cooperative pause; it must never fabricate
a completed native parent return or retain native inspection handles.
"""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import time
import run_dap_current_broker as live
import run_dap_native_finish as finish


def refresh_wasm_queries(client, thread):
    """Real GC writes and both query orders in one DAP batch, without idle polls."""
    state = {"threadId": thread, "selection": "globals", "start": 3, "count": 1}
    members = {"threadId": thread, "selection": "globals", "root": 3, "path": [], "count": 1}
    records = []
    previous = 9
    for order, value in ((["uwvm/wasmState", "uwvm/wasmMembers"], 42),
                         (["uwvm/wasmMembers", "uwvm/wasmState"], 43)):
        before = client.request("uwvm/wasmState", state)["body"]["variables"][0]
        old_object = before["variablesReference"]
        live.require(old_object > 0, "actual nonnull Wasm GC global", before)
        copied = client.request("variables", {"variablesReference": old_object})["body"]["variables"]
        live.require(len(copied) == 1 and copied[0]["value"] == str(previous), "actual pre-write GC field", copied)
        old_frame = live.physical_frame(client, thread)["id"]
        batch = [("evaluate", {"context": "repl", "expression":
                  f"set wasm member {thread} globals 0 3 at 0 bits i32 {value:x}"})]
        batch += [(name, state if name == "uwvm/wasmState" else members) for name in order]
        numbers = client.queue(batch)
        replies = [client.response(number) for number in numbers]
        live.require(replies[0]["success"] and "applied=1" in replies[0]["body"]["result"],
                     "actual GC member mutation", replies[0])
        live.require(all(reply["success"] for reply in replies[1:3]), "first fresh query must succeed without retry", replies)
        by_name = dict(zip(order, replies[1:3]))
        rows = by_name["uwvm/wasmMembers"]["body"]["variables"]
        live.require(len(rows) == 1 and rows[0]["value"] == str(value), "actual changed GC field through fresh root", rows)
        fresh_object = by_name["uwvm/wasmState"]["body"]["variables"][0]["variablesReference"]
        live.require(fresh_object > old_object, "session-unique fresh GC display reference")
        copied = client.request("variables", {"variablesReference": fresh_object})["body"]["variables"]
        live.require(len(copied) == 1 and copied[0]["value"] == str(value), "actual fresh object expansion", copied)
        numbers = client.queue([("variables", {"variablesReference": old_object}), ("scopes", {"frameId": old_frame})])
        stale = [client.response(number) for number in numbers]
        live.require(all(not reply["success"] for reply in stale), "GC write must retire old object/frame IDs", stale)
        records.append({"order": order, "old_value": previous, "new_value": value,
                        "responses": replies, "stale_responses": stale})
        previous = value
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", required=True, type=Path)
    parser.add_argument("--wasm", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--policy", required=True, choices=("instruction", "unwind"))
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--wasm-query-refresh", action="store_true", help="require the real GC refresh fixture")
    args = parser.parse_args()
    live.require(sys.platform == "linux" and os.uname().machine == "x86_64", "Linux x86_64 required")
    root = Path(__file__).resolve().parents[2]
    args.out.mkdir(mode=0o700)
    paths = (args.uwvm, args.wasm, Path(__file__), Path(finish.__file__), Path(live.__file__),
        root / "tools/debug/dap_adapter.py", root / "tools/debug/secure_server.py",
        root / "test/0017.runtime/run_debug_source_step_cli.py",
        root / "test/0017.runtime/run_debug_source_inline_metadata_cli.py")
    pins = {str(path): live.sha(path) for path in paths}
    spin, body = live.source_cli.metadata_cli.function(args.wasm, "spin")
    groups, expression = live.source_cli.metadata_cli.u32(body, 0, len(body))
    live.require(groups == 0 and body[expression:expression + 4] == b"\x03\x40\x23\x01",
                 "fixed spin loop starts with the real count global", body)
    mode = ["-Raot"] if args.ros else ["-Rcc", "jit", "-Rcm", "full"]
    vm = ["-m", "run", *mode, "-Rct", "0", "-Rllvm-call-stack", args.policy,
          "-Rllvm-exception-dispatch", "native-unwind", "-Rllvm-cache-path", "disable", "--run", str(args.wasm)]
    if args.wasm_query_refresh:
        vm[:0] = ["--wasm-feature-enable-gc", "--wasm-feature-enable-function-references",
                  "--wasm-feature-enable-reference-types"]
    server = live.BrokerSession(args.uwvm, root / "tools/debug/secure_server.py",
        root / "tools/debug/dap_adapter.py", vm, args.out)
    record = {"passed": False, "pins": pins,
        "scope": "actual native nonreturning finish deadline, retained Wasm pause, current-watch and original-owner cleanup"}
    try:
        client = server.connect("native")
        current, point = live.breakpoint_begin(client, spin, 2)
        client.evaluate(f"delete {point}")
        thread = current["thread"]
        for _ in range(64):
            number = client.queue([("stepIn", {"threadId": thread})])[0]
            reply = client.response(number)
            after = live.actual_location(client)
            if reply["success"]:
                live.require(after["stop"] > current["stop"] and after["function"] == spin,
                             "actual native spin capture", after)
                finish.native_pc(after)
                current = after
                break
            live.require(after == current, "refused SI moved actual spin")
            client.evaluate(f"step wasm {thread}")
            current = live.actual_location(client)
            live.require(current["function"] == spin, "explicit fixture search escaped spin", current)
        else:
            raise AssertionError("genuine native spin trap required")
        frame, values, code = finish.native_handles(client, current)
        record["before_timeout"] = current
        event_begin = len(client.events)
        start = time.monotonic()
        numbers = client.queue([
            ("stepOut", {"threadId": thread, "granularity": "instruction"}),
            ("variables", {"variablesReference": values}),
            ("evaluate", {"context": "watch", "frameId": frame["id"], "expression": "$pc"}),
            ("disassemble", {"memoryReference": code, "instructionCount": 1}),
            ("scopes", {"frameId": frame["id"]})])
        replies = [client.response(number) for number in numbers]
        elapsed = time.monotonic() - start
        live.require(1 <= elapsed < 15, "actual bounded return deadline", elapsed)
        live.require(all(reply["success"] is False for reply in replies[1:]),
                     "timeout retired previously valid native handles immediately", replies)
        after = live.actual_location(client)
        live.require(after["stop"] > current["stop"] and after["function"] == spin and
                     after["epoch"] == current["epoch"] and "native-pc=" not in after["raw"] and
                     "stopped: pause" in after["raw"], "timeout must retain a real cooperative callee pause", after)
        outputs = [event.get("body", {}).get("output", "") for event in client.events[event_begin:]
                   if event.get("event") == "output"]
        live.require(not any(output.startswith("native finish ") for output in outputs),
                     "nonreturning deadline fabricated a completed native return", outputs)
        diagnostics = [output for output in outputs if output.startswith("pause/step timed out;")]
        live.require(len(diagnostics) == 1, "DAP must preserve the actual controller timeout diagnostic", outputs)
        record.update({"after_timeout": after, "deadline_seconds": elapsed,
                       "timeout_diagnostic": diagnostics[0],
                       "step_response": replies[0], "stale_responses": replies[1:]})
        # A second native finish at the cooperative pause must not reuse the
        # retired return event/capture, even with the same Wasm function/epoch.
        client.request("stepOut", {"threadId": thread}, success=False)
        live.require(live.actual_location(client) == after, "old native return authority survived timeout")
        if args.wasm_query_refresh:
            record["wasm_query_refresh"] = refresh_wasm_queries(client, thread)
        changed = client.evaluate(f"set wasm global 0 0 {thread} bits i32 0")
        live.require(b"applied=true" in changed or b"applied=1" in changed,
                     "only an actual Wasm keep global exits the spin", changed)
        stopped = client.evaluate("continue")
        for _ in range(20):
            if b"guest exited: 0" in stopped:
                break
            stopped = client.evaluate("wait")
        else:
            raise AssertionError("actual guest did not exit after timeout and real Wasm mutation")
        record["actual_guest_exit"] = stopped.decode()
        # DAP events follow their response asynchronously. Drain actual framed
        # messages through terminated before judging the exit-code witness;
        # merely reading the wait response does not receive later events.
        for _ in range(16):
            if any(event.get("event") == "terminated" for event in client.events):
                break
            client.absorb(client.next_message())
        else:
            raise AssertionError("bounded actual DAP terminal-event drain")
        live.require(any(event.get("event") == "exited" and event.get("body", {}).get("exitCode") == 0
                         for event in client.events), "actual DAP exited event")
        record["passed"] = True
    finally:
        try:
            record["cleanup"] = server.close()
        except BaseException as error:
            record["passed"] = False
            record["cleanup_error"] = repr(error)
            raise
        finally:
            record["pins_after"] = {name: live.sha(Path(name)) for name in pins}
            live.require(record["pins_after"] == pins, "immutable native timeout inputs changed")
            (args.out / "native-finish-timeout.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"run_dap_native_finish_timeout: PASS policy={args.policy} actual-deadline=yes "
          "cooperative-pause-retained=yes stale-references-rejected=4 actual-guest-exit=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

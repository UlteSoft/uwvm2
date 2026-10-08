#!/usr/bin/env python3
"""Genuine Linux broker/DAP native stepOut acceptance, keeper cgroup only.

Recursive parents must come from actual kernel return stops. Queue old handles
immediately behind stepOut, without an intervening status request or idle poll.
Consumes an independently qualified full interpreter/full LLVM runtime product.
"""
import argparse
import json
import os
from pathlib import Path
import re
import sys
import run_dap_current_broker as live


def native_pc(location):
    pcs = re.findall(r"(?m)^  native-pc=0x([0-9a-fA-F]{1,16})$", location["raw"])
    live.require(len(pcs) == 1 and int(pcs[0], 16) > 0, "one actual native PC", location)
    return int(pcs[0], 16)


def native_handles(client, current):
    frame = live.physical_frame(client, current["thread"])
    code = frame.get("instructionPointerReference", "")
    live.require(code.startswith("uwvm-native-stop:") and "source" not in frame and
                 frame["line"] == 0 and frame.get("canRestart") is False and
                 f"function={current['function']} " in frame["name"],
                 "native parent acquired a source/cooperative position", frame)
    scopes = client.request("scopes", {"frameId": frame["id"]})["body"]["scopes"]
    live.require([row["name"] for row in scopes] == ["Registers"], "native scope boundary", scopes)
    reference = scopes[0]["variablesReference"]
    values = client.request("variables", {"variablesReference": reference})["body"]["variables"]
    live.require(all("memoryReference" not in row for row in values), "register memory capability")
    hidden = [row for row in values if row["name"] in ("rsp", "rbp")]
    live.require(len(hidden) == 2 and all(row["value"] == "unavailable" for row in hidden),
                 "native stack/frame address exposure", values)
    instructions = client.request("disassemble", {
        "memoryReference": code, "instructionCount": 1})["body"]["instructions"]
    live.require(len(instructions) == 1, "one bounded current instruction row", instructions)
    row = instructions[0]
    if row.get("presentationHint") == "invalid":
        # Private glue/spill/control instructions must remain hidden even at a
        # genuine native stop. A current opaque handle can return a filler.
        live.require(row == {"address": "-1", "instruction": "<unavailable instruction>",
                             "presentationHint": "invalid"}, "exact safe unavailable row", row)
    else:
        live.require(int(row["address"], 16) == native_pc(current),
                     "visible instruction must name the actual parent PC", row)
    for expression, expected in (("$pc", native_pc(current)), ("$sp", None), ("$fp", None)):
        watch = client.request("evaluate", {
            "context": "watch", "frameId": frame["id"], "expression": expression})["body"]
        live.require(watch.get("variablesReference") == 0 and "memoryReference" not in watch and
                     watch.get("presentationHint", {}).get("attributes") == ["readOnly"],
                     "native watch must remain a copied read-only value", watch)
        if expected is None:
            live.require(watch["result"] == "unavailable", "native stack/frame watch exposure", watch)
        else:
            live.require(int(watch["result"], 16) == expected, "watch PC disagrees with actual kernel stop", watch)
    return frame, reference, code


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", required=True, type=Path)
    parser.add_argument("--wasm", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--policy", required=True, choices=("instruction", "unwind"))
    parser.add_argument("--ros", action="store_true")
    args = parser.parse_args()
    live.require(sys.platform == "linux" and os.uname().machine == "x86_64",
                 "qualified Linux x86_64 kernel required")
    root = Path(__file__).resolve().parents[2]
    args.out.mkdir(mode=0o700)
    pin_paths = (args.uwvm, args.wasm, root / "tools/debug/dap_adapter.py",
        root / "tools/debug/secure_server.py", Path(__file__),
        root / "test/0018.debugger/run_dap_current_broker.py",
        root / "test/0017.runtime/run_debug_source_step_cli.py",
        root / "test/0017.runtime/run_debug_source_inline_metadata_cli.py")
    pins = {str(path): live.sha(path) for path in pin_paths}
    recursive = live.source_cli.metadata_cli.function(args.wasm, "recursive")[0]
    start = live.source_cli.metadata_cli.function(args.wasm, "_start")[0]
    mode = ["-Raot"] if args.ros else ["-Rcc", "jit", "-Rcm", "full"]
    vm = ["-m", "run", *mode, "-Rct", "0", "-Rllvm-call-stack", args.policy,
          "-Rllvm-exception-dispatch", "native-unwind", "-Rllvm-cache-path", "disable",
          "--run", str(args.wasm)]
    server = live.BrokerSession(args.uwvm, root / "tools/debug/secure_server.py",
        root / "tools/debug/dap_adapter.py", vm, args.out)
    record = {"passed": False, "pins": pins, "native_finish": [],
              "stale_references_rejected": 0, "positive_native_watch_queries": 0, "native_search_refusals": 0,
              "scope": "actual recursive Wasm JIT native parents through broker/DAP; Linux x86_64 only"}
    try:
        client = server.connect("native")
        # An asynchronous attach can pause halfway through existing recursion.
        # First reach the recurring root constant, independently checked in the
        # actual Code body, so the next call starts with depth exactly two.
        _, body = live.source_cli.metadata_cli.function(args.wasm, "_start")
        groups, expression = live.source_cli.metadata_cli.u32(body, 0, len(body))
        live.require(groups == 0 and body[expression:expression + 4] == b"\x03\x40\x41\x02",
                     "fixed root loop and depth-two constant oracle", body)
        current, root_point = live.breakpoint_begin(client, start, 2)
        record["root_synchronization_stop"] = current
        client.evaluate(f"delete {root_point}")
        point_reply = client.evaluate(f"break 0 {recursive} 0")
        identifier = re.search(rb"breakpoint ([0-9]+)", point_reply)
        live.require(identifier is not None, "actual recursive breakpoint", point_reply)
        point = int(identifier[1])
        # All three activations execute the same function bytes. Distinct stop
        # identities and actual return traps, rather than a changed PC, prove
        # physical parent movement when recursive return addresses are equal.
        for depth in range(3):
            client.evaluate("continue")
            for _ in range(20):
                stopped = client.evaluate("wait")
                if b"stopped: breakpoint" in stopped:
                    break
            else:
                raise AssertionError("deeper actual recursive breakpoint timeout")
            after = live.actual_location(client)
            live.require(after["function"] == recursive and after["offset"] == 0 and
                         after["stop"] > current["stop"], "actual recursive incarnation", after)
            current = after
        client.evaluate(f"delete {point}")
        thread = current["thread"]
        client.request("stepOut", {"threadId": thread}, success=False)
        live.require(live.actual_location(client) == current, "cooperative finish refusal moved guest")
        for _ in range(64):
            number = client.queue([("stepIn", {"threadId": thread})])[0]
            reply = client.response(number)
            after = live.actual_location(client)
            if reply["success"]:
                live.require(after["stop"] > current["stop"] and after["function"] == recursive,
                             "native SI must stay in the recursive child", after)
                native_pc(after)
                current = after
                break
            live.require(after == current, "unproved native SI moved the guest")
            record["native_search_refusals"] += 1
            # This explicit fixture search grants no production native fallback.
            client.evaluate(f"step wasm {thread}")
            current = live.actual_location(client)
            live.require(current["function"] == recursive, "search escaped recursive child", current)
        else:
            raise AssertionError("genuine native child capture required")
        for expected, granularity in zip((recursive, recursive, start), (None, "statement", "instruction")):
            frame, values, code = native_handles(client, current)
            record["positive_native_watch_queries"] += 3
            event_begin = len(client.events)
            arguments = {"threadId": thread}
            if granularity is not None:
                arguments["granularity"] = granularity
            numbers = client.queue([
                ("stepOut", arguments),
                ("variables", {"variablesReference": values}),
                ("evaluate", {"context": "watch", "frameId": frame["id"], "expression": "$pc"}),
                ("disassemble", {"memoryReference": code, "instructionCount": 1}),
                ("scopes", {"frameId": frame["id"]})])
            replies = [client.response(number) for number in numbers]
            live.require(replies[0].get("success") is True and
                         all(reply.get("success") is False for reply in replies[1:]),
                         "real finish and immediately queued stale-handle retirement", replies)
            after = live.actual_location(client)
            live.require(after["stop"] > current["stop"] and after["function"] == expected and
                         after["thread"] == thread and after["epoch"] == current["epoch"] and
                         "stopped: native instruction step" in after["raw"],
                         "finish must replace the actual child with its physical parent", after)
            outputs = [event["body"].get("output", "") for event in client.events[event_begin:]
                       if event.get("event") == "output"]
            movements = [re.fullmatch(r"native finish 0x([0-9a-fA-F]+) -> 0x([0-9a-fA-F]+)\n?", output)
                         for output in outputs]
            movements = [match for match in movements if match is not None]
            live.require(len(movements) == 1 and
                         tuple(int(value, 16) for value in movements[0].groups()) ==
                         (native_pc(current), native_pc(after)),
                         "DAP output must describe the actual finish PCs", outputs)
            fresh_frame, fresh_values, fresh_code = native_handles(client, after)
            record["positive_native_watch_queries"] += 3
            live.require(fresh_frame["id"] != frame["id"] and fresh_values != values and fresh_code != code,
                         "parent must receive fresh handles")
            client.request("readMemory", {"memoryReference": fresh_code, "count": 8}, success=False)
            client.request("writeMemory", {"memoryReference": fresh_code, "data": "AAAAAA=="}, success=False)
            live.require(live.actual_location(client) == after, "read-only/refused queries moved parent")
            record["native_finish"].append({"before": current, "after": after,
                "granularity": granularity,
                "immediately_queued_stale_responses": replies[1:]})
            record["stale_references_rejected"] += 4
            current = after
        client.request("stepOut", {"threadId": thread}, success=False)
        live.require(live.actual_location(client) == current, "root finish exposed/moved into host caller")
        record["root_finish_refused"] = True
        record["passed"] = True
    finally:
        try:
            record["cleanup"] = server.close()
        except BaseException as error:
            record["passed"] = False
            record["cleanup_error"] = repr(error)
            raise
        finally:
            record["pins_after"] = {path: live.sha(Path(path)) for path in pins}
            live.require(record["pins_after"] == pins, "qualified immutable inputs changed")
            (args.out / "native-finish.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"run_dap_native_finish: PASS policy={args.policy} real-native-finish=3 "
          "immediate-stale-references-rejected=12 positive-native-watch-queries=18 "
          "root-finish-refused=yes public-native-stack-bytes=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

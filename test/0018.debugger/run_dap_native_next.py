#!/usr/bin/env python3
"""Genuine Linux broker/DAP native-next acceptance, keeper cgroup only.

Uses the qualified full runtime product and synchronous kernel native stops.
Formatter-shaped unit replies cannot satisfy this driver's positive NI counts.
"""
import argparse
import json
import os
from pathlib import Path
import sys
import run_dap_current_broker as live


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
    pins = {str(p): live.sha(p) for p in (args.uwvm, args.wasm,
        root / "tools/debug/dap_adapter.py", root / "tools/debug/secure_server.py",
        Path(__file__), root / "test/0018.debugger/run_dap_current_broker.py")}
    mode = ["-Raot"] if args.ros else ["-Rcc", "jit", "-Rcm", "full"]
    vm = ["-m", "run", *mode, "-Rct", "0", "-Rllvm-call-stack", args.policy,
          "-Rllvm-exception-dispatch", "native-unwind", "-Rllvm-cache-path", "disable",
          "--run", str(args.wasm)]
    server = live.BrokerSession(args.uwvm, root / "tools/debug/secure_server.py",
        root / "tools/debug/dap_adapter.py", vm, args.out)
    record = {"passed": False, "pins": pins, "native_next": [], "refused_native": 0,
              "scope": "real top Wasm JIT native next; root finish refusal; no other OS qualification"}
    try:
        client = server.connect("native")
        current, point = live.breakpoint_begin(client, 0, 6)
        client.evaluate(f"delete {point}")
        # A cooperative Wasm row is not permission for NI, even at native level.
        client.request("next", {"threadId": current["thread"]}, success=False)
        unchanged = live.actual_location(client)
        live.require(unchanged == current, "refused cooperative NI moved the guest", unchanged)
        thread = current["thread"]
        for attempt in range(192):
            if "native-pc=" not in current["raw"]:
                number = client.queue([("stepIn", {"threadId": thread})])[0]
                reply = client.response(number)
                fresh = live.actual_location(client)
                if not reply["success"]:
                    live.require(fresh == current, "refused native SI changed actual stop")
                    client.evaluate(f"step wasm {thread}")
                    current = live.actual_location(client)
                    continue
                live.require("native-pc=" in fresh["raw"] and fresh["stop"] > current["stop"],
                             "SI did not establish a genuine native trap", fresh)
                current = fresh
            frame = live.physical_frame(client, thread)
            old_code = frame.get("instructionPointerReference", "")
            live.require(old_code.startswith("uwvm-native-stop:") and "source" not in frame,
                         "native top frame acquired a source/cooperative position", frame)
            scopes = client.request("scopes", {"frameId": frame["id"]})["body"]["scopes"]
            live.require([r["name"] for r in scopes] == ["Registers"], "native scope boundary", scopes)
            old_values = scopes[0]["variablesReference"]
            values = client.request("variables", {"variablesReference": old_values})["body"]["variables"]
            live.require(all("memoryReference" not in r for r in values), "native register memory capability")
            live.require(all(r["value"] == "unavailable" for r in values if r["name"] in ("rsp", "rbp")),
                         "native stack/frame address exposure", values)
            granularity = (None, "statement", "instruction")[len(record["native_next"]) % 3]
            arguments = {"threadId": thread}
            if granularity is not None:
                arguments["granularity"] = granularity
            number = client.queue([("next", arguments)])[0]
            reply = client.response(number)
            after = live.actual_location(client)
            if not reply["success"]:
                live.require(after == current, "refused NI changed the real native trap", reply)
                record["refused_native"] += 1
                client.evaluate(f"step wasm {thread}")
                current = live.actual_location(client)
                continue
            live.require(after["stop"] > current["stop"] and "native-pc=" in after["raw"] and
                         "stopped: native instruction step" in after["raw"],
                         "DAP NI success without a new genuine native stop", (reply, after))
            record["native_next"].append({"before": current, "after": after, "granularity": granularity})
            client.request("variables", {"variablesReference": old_values}, success=False)
            client.request("evaluate", {"context": "watch", "frameId": frame["id"], "expression": "$pc"}, success=False)
            client.request("disassemble", {"memoryReference": old_code, "instructionCount": 1}, success=False)
            live.require(live.actual_location(client) == after, "stale-reference refusals moved the inferior")
            current = after
            if len(record["native_next"]) == 6:
                break
        live.require(len(record["native_next"]) == 6, "six genuine DAP NI stops required", record)
        client.request("stepOut", {"threadId": thread}, success=False)
        live.require(live.actual_location(client) == current, "root native finish refusal moved inferior")
        record["passed"] = True
    finally:
        try:
            record["cleanup"] = server.close()
        except BaseException as error:
            record["passed"] = False
            record["cleanup_error"] = repr(error)
            raise
        finally:
            record["pins_after"] = {p: live.sha(Path(p)) for p in pins}
            live.require(record["pins_after"] == pins, "qualified immutable inputs changed")
            (args.out / "native-next.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"run_dap_native_next: PASS policy={args.policy} real-native-next=6 stale-references-rejected=18 public-native-stack-bytes=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

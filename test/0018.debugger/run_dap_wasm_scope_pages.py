#!/usr/bin/env python3
"""Seven large typed scopes through the actual Linux JIT and authenticated broker."""
import argparse
import importlib.util
import json
import os
import re
from pathlib import Path
import sys
import time
import run_dap_current_broker as live
from run_dap_frame_scope_lifetime import Dispatch, ScheduledBroker

TITLES = {
    "locals": "Typed Wasm locals", "operands": "Wasm operand stack (last safepoint; may differ from native state)",
    "saved": "Wasm saved if parameters", "controls": "Wasm control stack", "handlers": "Wasm handler clauses",
    "globals": "Wasm globals", "table": "Wasm table 0",
}
TOTALS = {"locals":161,"operands":160,"saved":160,"controls":165,"handlers":160,"globals":161,"table":161}


class PageBroker(ScheduledBroker):
    def __init__(self, channel):
        super().__init__(channel); self.after_page = None
    def request(self, command):
        result = super().request(command)
        if command.split()[0] in (*TITLES,"locals") and self.after_page is not None:
            hook = self.after_page
            if hook(): self.after_page = None
        return result


def scopes(client, thread):
    frames = client.request("stackTrace",{"threadId":thread,"startFrame":0,"levels":1})["body"]["stackFrames"]
    live.require(len(frames) == 1 and frames[0]["name"].startswith("Wasm frame 0 "),"actual canonical Wasm frame zero",frames)
    frame = frames[0]
    rows = client.request("scopes",{"frameId":frame["id"]})["body"]["scopes"]
    return {selected:next(r["variablesReference"] for r in rows if r["name"] == title) for selected,title in TITLES.items()}


def recurring_stop(client, function, offset):
    # Already stopped after the external step: pause is a running-state
    # command. Reinstall the checked recurring breakpoint and continue.
    reply = client.evaluate(f"break 0 {function} {offset}")
    match = re.search(rb"breakpoint ([0-9]+)",reply)
    live.require(match is not None,"actual recurring breakpoint registration",reply)
    point = int(match[1]); client.evaluate("continue")
    for _ in range(20):
        reply = client.evaluate("wait")
        if b"stopped: breakpoint" in reply: break
    else: raise AssertionError("actual recurring scope breakpoint timeout")
    current = live.actual_location(client)
    live.require((current["function"],current["offset"]) == (function,offset),"actual recurring scope location",current)
    client.evaluate(f"delete {point}")
    return current


def verify(selected, values, indices):
    indices = list(indices)
    prefix = {"locals":"local","operands":"operand","saved":"saved-parameter","controls":"control","handlers":"handler","globals":"global","table":"table 0 element"}[selected]
    live.require([v["name"] for v in values] == [f"{prefix} {i}" for i in indices],"actual typed original scope indices",(selected,values,indices))
    for i, value in zip(indices,values):
        live.require("memoryReference" not in value and "evaluateName" not in value and
                     value["presentationHint"]["attributes"] == ["readOnly"],"scope host memory capability",value)
        if selected in ("locals","globals") and i == 160:
            live.require(value["variablesReference"] > 0 and value.get("indexedVariables") == 1,"actual GC root from paged typed scope",value)
        elif selected == "table":
            live.require(value["value"] == "function module=0 index=0" and value["variablesReference"] == 0,"actual funcref table value",value)
        elif selected == "controls":
            live.require("kind=" in value["value"] and value["variablesReference"] == 0,"actual lexical control DATA",value)
        elif selected == "handlers":
            live.require("catch=catch-all" in value["value"] and value["variablesReference"] == 0,"actual lexical handler DATA",value)
        elif selected != "globals" or i >= 3:
            base = 1000 if selected == "locals" else 2000 if selected == "globals" else 3000
            live.require(value["value"] == str(base+i) and value["variablesReference"] == 0,"actual typed scope scalar value",value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm",required=True,type=Path); parser.add_argument("--wasm",required=True,type=Path)
    parser.add_argument("--out",required=True,type=Path); parser.add_argument("--policy",required=True,choices=("instruction","unwind"))
    parser.add_argument("--ros",action="store_true"); args = parser.parse_args()
    live.require(sys.platform == "linux" and os.uname().machine == "x86_64","Linux x86_64 required")
    args.out.mkdir(mode=0o700); root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location("actual_scope_pages_dap",root/"tools/debug/dap_adapter.py")
    dap = importlib.util.module_from_spec(spec); spec.loader.exec_module(dap)
    paths = (args.uwvm,args.wasm,Path(__file__),Path(live.__file__),root/"tools/debug/dap_adapter.py",root/"tools/debug/secure_server.py",
        root/"test/0018.debugger/run_dap_frame_scope_lifetime.py",root/"test/0017.runtime/run_debug_source_step_cli.py",root/"test/0017.runtime/run_debug_source_inline_metadata_cli.py")
    pins = {str(p):live.sha(p) for p in paths}
    function,body = live.source_cli.metadata_cli.function(args.wasm,"spin")
    groups,cursor = live.source_cli.metadata_cli.u32(body,0,len(body)); live.require(groups == 2,"actual typed fixture local groups")
    count,cursor = live.source_cli.metadata_cli.u32(body,cursor,len(body)); live.require(count == 160 and body[cursor] == 0x7f,"actual 160 i32 local declarations")
    cursor += 1; count,cursor = live.source_cli.metadata_cli.u32(body,cursor,len(body))
    live.require(count == 1 and body[cursor] == 0x63,"actual nullable GC local declaration")
    cursor += 1; type_index,cursor = live.source_cli.metadata_cli.u32(body,cursor,len(body)); live.require(type_index == 1,"actual cell type index")
    loop = body.find(b"\x03\x40\x23\x01",cursor)
    live.require(loop > cursor and body.find(b"\x03\x40\x23\x01",loop+1) == -1,"unique recurring typed scope safepoint")
    target = loop+2-cursor
    mode = ["-Raot"] if args.ros else ["-Rcc","jit","-Rcm","full"]
    vm = ["--wasm-feature-enable-gc","--wasm-feature-enable-function-references","--wasm-feature-enable-reference-types","--wasm-feature-enable-exceptions",
        "-m","run",*mode,"-Rct","0","-Rllvm-call-stack",args.policy,"-Rllvm-exception-dispatch","native-unwind","-Rllvm-cache-path","disable","--run",str(args.wasm)]
    server = live.BrokerSession(args.uwvm,root/"tools/debug/secure_server.py",root/"tools/debug/dap_adapter.py",vm,args.out)
    broker = None; record = {"passed":False,"pins":pins,"pages":[],"all_scopes":{},"stale":{},"mid_copy":{},"startup_settle_seconds":20}
    try:
        # This large typed-site fixture compiles before adopting the inherited
        # debugger endpoint. Keep startup separate from the unchanged 6-second
        # production RPC deadline. The queued-first-command test is separate.
        time.sleep(record["startup_settle_seconds"])
        live.require(server.child.poll() is None,"large typed fixture exited during startup",server.child.poll())
        broker = PageBroker(dap.UnixBroker(str(server.directory))); client = Dispatch(dap,broker)
        client.adapter.step_level = "wasm"
        current,point = live.breakpoint_begin(client,function,target); client.evaluate(f"delete {point}")
        thread = current["thread"]; refs = scopes(client,thread); gc_refs = {}
        for selected in TITLES:
            ref,total = refs[selected],TOTALS[selected]
            for first,count in ((129,8),(63,80),(total-1,64),(total,4),((1<<64)-1,1)):
                reply = client.request("variables",{"variablesReference":ref,"start":first,"count":count,"filter":"indexed"})
                verify(selected,reply["body"]["variables"],range(first,min(first+count,total)))
                record["pages"].append({"selection":selected,"start":first,"count":count,"response":reply})
            reply = client.request("variables",{"variablesReference":ref,"start":total-3,"count":0})
            verify(selected,reply["body"]["variables"],range(total-3,total))
            reply = client.request("variables",{"variablesReference":ref})
            verify(selected,reply["body"]["variables"],range(total)); record["all_scopes"][selected] = reply
            if selected in ("locals","globals"):
                gc = reply["body"]["variables"][-1]["variablesReference"]
                gc_refs[selected] = gc
                members = client.request("variables",{"variablesReference":gc})["body"]["variables"]
                live.require(len(members) == 1 and members[0]["value"] == "42" and "memoryReference" not in members[0],"GC original root expansion after scope paging",members)
                record.setdefault("gc_members",{})[selected] = members
            before = len(broker.commands); reply = client.request("variables",{"variablesReference":ref,"filter":"named"})
            live.require(reply["body"]["variables"] == [] and all(r["command"] == "status" for r in broker.commands[before:]),"named filter unexpectedly borrows typed scope")
        record["external_step"] = broker.request(f"step wasm {thread}")
        for selected,ref in refs.items():
            refusal = client.request("variables",{"variablesReference":ref},success=False)
            live.require("body" not in refusal,"retired typed scope published DATA",refusal); record["stale"][selected] = refusal
        for selected,ref in gc_refs.items():
            refusal = client.request("variables",{"variablesReference":ref},success=False)
            live.require("body" not in refusal,"retired paged scope GC root published DATA",refusal)
            record.setdefault("stale_gc",{})[selected] = refusal
        # Reacquire the SAME recurring safepoint for each real mid-copy change;
        # stepping an arbitrary instruction could change operand cardinality.
        for selected in TITLES:
            current = recurring_stop(client,function,target)
            thread = current["thread"]; refs = scopes(client,thread); copied = 0
            def advance():
                nonlocal copied
                copied += 1
                if copied == 2:
                    broker.after_page = None
                    record["mid_copy"][selected] = {"external_step":broker.request(f"step wasm {thread}")}
                    return True
                return False
            broker.after_page = advance
            refusal = client.request("variables",{"variablesReference":refs[selected],"start":63,"count":80},success=False)
            live.require("body" not in refusal and copied == 2,"partial typed scope page published across actual stop",refusal)
            record["mid_copy"][selected]["refusal"] = refusal
        current = recurring_stop(client,function,target)
        refs = scopes(client,current["thread"])
        for selected,ref in refs.items():
            reply = client.request("variables",{"variablesReference":ref,"start":129,"count":8})
            verify(selected,reply["body"]["variables"],range(129,137))
        client.evaluate(f"set wasm global 0 0 {current['thread']} bits i32 0"); result = client.evaluate("continue")
        for _ in range(20):
            if b"guest exited: 0" in result: break
            result = client.evaluate("wait")
        live.require(b"guest exited: 0" in result,"actual typed scope fixture natural logical exit",result)
        record.update(passed=True,actual_guest_exit=result.decode(),totals=TOTALS)
    finally:
        if "client" in locals(): record["requests"] = client.requests
        if broker is not None: record["broker_commands"] = broker.commands; broker.close()
        try:
            if record["passed"]:
                try: server.child.wait(timeout=10)
                except BaseException as error:
                    record.update(passed=False,natural_exit_wait_error=repr(error)); raise
        finally:
            try: record["cleanup"] = server.close()
            finally:
                record["pins_after"] = {n:live.sha(Path(n)) for n in pins}
                live.require(record["pins_after"] == pins,"immutable actual scope page inputs changed")
                (args.out/"wasm-scope-pages.json").write_text(json.dumps(record,indent=2)+"\n")
    print(f"run_dap_wasm_scope_pages: PASS policy={args.policy} seven-scopes>64 stale=16 natural-exit=0")
    return 0


if __name__ == "__main__": raise SystemExit(main())

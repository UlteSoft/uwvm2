#!/usr/bin/env python3
"""Four standard WASIp1 scopes through the actual authenticated Linux JIT broker."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import sys
import time
import run_dap_current_broker as live
from run_dap_frame_scope_lifetime import Dispatch, ScheduledBroker
from run_dap_wasm_scope_pages import recurring_stop

TITLES = {"args":"WASIp1 arguments", "env":"WASIp1 environment", "fds":"WASIp1 descriptors", "preopens":"WASIp1 preopens"}


class PageBroker(ScheduledBroker):
    def __init__(self, channel): super().__init__(channel); self.after_page=None
    def request(self,command):
        result=super().request(command)
        if command.startswith("info wasip1 ") and self.after_page:
            if self.after_page(): self.after_page=None
        return result


def scopes(client,thread):
    frames=client.request("stackTrace",{"threadId":thread,"startFrame":0,"levels":1})["body"]["stackFrames"]
    live.require(len(frames)==1 and frames[0]["name"].startswith("Wasm frame 0 "),"actual cooperative physical frame",frames)
    rows=client.request("scopes",{"frameId":frames[0]["id"]})["body"]["scopes"]
    found={selected:[s for s in rows if s["name"]==title] for selected,title in TITLES.items()}
    live.require(all(len(v)==1 and v[0]["expensive"] for v in found.values()),"four standard WASIp1 scopes are missing",rows)
    return {s:v[0]["variablesReference"] for s,v in found.items()}


def custom(client,selected):
    cursor=0; rows=[]; total=None
    for _ in range(2048):
        reply=client.request("uwvm/wasip1State",{"selection":selected,"moduleId":0,"start":cursor,"count":64})["body"]
        live.require(reply["available"],"actual WASIp1 environment admission",reply)
        if total is None: total=reply["total"]
        live.require(reply["total"]==total,"actual custom WASIp1 total changed")
        rows.extend(reply["variables"])
        if not reply["more"]:
            live.require(len(rows)==total,"actual custom WASIp1 scan incomplete",(len(rows),total)); return rows
        live.require(reply["next"]>cursor,"actual custom WASIp1 cursor did not advance",reply)
        cursor=reply["next"]
    raise AssertionError("actual custom WASIp1 scan budget")


def verify(values,expected,out):
    live.require(values==expected,"standard WASIp1 scope differs from actual guest data",(values,expected))
    for row in values:
        live.require(row["variablesReference"]==0 and "memoryReference" not in row and "evaluateName" not in row and
                     row["presentationHint"]["attributes"]==["readOnly"],"WASIp1 scope has host authority",row)
        live.require(str(out) not in row["value"] and "native-handle=" not in row["value"] and "host-path=" not in row["value"],"WASIp1 scope exposes host resource",row)


def imported_function(path, export):
    """Read this validated fixture's actual exported body, including its import."""
    metadata=live.source_cli.metadata_cli
    imported=0; target=None; code=None
    for kind,payload in metadata.sections(path):
        if kind==2:
            count,at=metadata.u32(payload,0,len(payload))
            live.require(count==1,"actual single WASIp1 function import")
            names=[]
            for _ in range(2):
                size,at=metadata.u32(payload,at,len(payload))
                live.require(size<=len(payload)-at,"actual import name bounds")
                names.append(payload[at:at+size]);at+=size
            live.require(names==[b"wasi_snapshot_preview1",b"args_sizes_get"] and at<len(payload) and payload[at]==0,"actual WASIp1 function import",names)
            _,at=metadata.u32(payload,at+1,len(payload))
            live.require(at==len(payload),"actual import section extent");imported=1
        elif kind==7:
            count,at=metadata.u32(payload,0,len(payload))
            for _ in range(count):
                size,at=metadata.u32(payload,at,len(payload))
                live.require(size<len(payload)-at,"actual export name bounds")
                name=payload[at:at+size];at+=size;tag=payload[at]
                index,at=metadata.u32(payload,at+1,len(payload))
                if name==export.encode() and tag==0:
                    live.require(target is None,"duplicate actual target export");target=index
            live.require(at==len(payload),"actual export section extent")
        elif kind==10: code=payload
    live.require(imported==1 and target is not None and code is not None,"actual WASIp1 fixture sections")
    count,at=metadata.u32(code,0,len(code));bodies=[]
    for _ in range(count):
        size,at=metadata.u32(code,at,len(code));live.require(size<=len(code)-at,"actual local function body bounds")
        bodies.append(code[at:at+size]);at+=size
    live.require(at==len(code) and imported<=target<imported+len(bodies),"actual exported local body index")
    return target,bodies[target-imported]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ("uwvm","wasm","out"): p.add_argument("--"+name,type=Path,required=True)
    p.add_argument("--policy",choices=("instruction","unwind"),required=True); p.add_argument("--ros",action="store_true")
    args=p.parse_args(); live.require(sys.platform=="linux" and os.uname().machine=="x86_64","Linux x86_64 required")
    args.out.mkdir(mode=0o700); mount=args.out/"owned-mount"; mount.mkdir(mode=0o700)
    root=Path(__file__).resolve().parents[2]
    spec=importlib.util.spec_from_file_location("actual_wasip1_scope_dap",root/"tools/debug/dap_adapter.py")
    dap=importlib.util.module_from_spec(spec); spec.loader.exec_module(dap)
    paths=(args.uwvm,args.wasm,Path(__file__),Path(live.__file__),root/"tools/debug/dap_adapter.py",root/"tools/debug/secure_server.py",
           root/"test/0018.debugger/run_dap_frame_scope_lifetime.py",root/"test/0018.debugger/run_dap_wasm_scope_pages.py",
           root/"test/0017.runtime/run_debug_source_step_cli.py",root/"test/0017.runtime/run_debug_source_inline_metadata_cli.py")
    pins={str(path):live.sha(path) for path in paths}
    function,body=imported_function(args.wasm,"spin")
    groups,cursor=live.source_cli.metadata_cli.u32(body,0,len(body)); live.require(groups==1,"actual WASIp1 fixture local groups")
    count,cursor=live.source_cli.metadata_cli.u32(body,cursor,len(body)); live.require(count==1 and body[cursor]==0x7f,"actual marker local")
    cursor+=1; loop=body.find(b"\x03\x40\x23\x01",cursor)
    live.require(loop>cursor and body.find(b"\x03\x40\x23\x01",loop+1)==-1,"unique actual WASIp1 recurring safepoint")
    target=loop+2-cursor
    mode=["-Raot"] if args.ros else ["-Rcc","jit","-Rcm","full"]
    settings=["--wasip1-global-noinherit-system-environment","--wasip1-global-force-args","160","wasip1 scope",*[f"argument{i:03}" for i in range(1,160)]]
    for i in range(160): settings.extend(("--wasip1-global-add-or-replace-environment",f"E{i:03}",f"value{i:03}"))
    for i in range(160): settings.extend(("--wasip1-global-mount-dir",f"/guest{i:03}",str(mount)))
    vm=["-m","run",*mode,"-Rct","0","-Rllvm-call-stack",args.policy,"-Rllvm-exception-dispatch","native-unwind","-Rllvm-cache-path","disable",*settings,"--run",str(args.wasm)]
    server=live.BrokerSession(args.uwvm,root/"tools/debug/secure_server.py",root/"tools/debug/dap_adapter.py",vm,args.out)
    broker=None; record={"passed":False,"pins":pins,"pages":[],"all_scopes":{},"stale":{},"mid_copy":{},"closed":[],"mutation_stale":{},"startup_settle_seconds":3}
    try:
        time.sleep(record["startup_settle_seconds"])
        live.require(server.child.poll() is None,"WASIp1 scope guest exited during startup",server.child.poll())
        broker=PageBroker(dap.UnixBroker(str(server.directory))); client=Dispatch(dap,broker);client.adapter.step_level="wasm"
        current,point=live.breakpoint_begin(client,function,target);client.evaluate(f"delete {point}")
        refs=scopes(client,current["thread"]); pre=custom(client,"preopens")
        live.require(len(pre)==160 and all(v["preopened"] for v in pre),"actual 160 guest preopens",pre)
        # Close actual guest descriptors to force sparse numbering. Every mount
        # names the same empty directory owned by this test, never broker files.
        for row in pre[::8]:
            reply=client.request("uwvm/wasip1Edit",{"operation":"closeDescriptor","moduleId":0,"descriptor":row["descriptor"],"expectedBase":row["rightsBase"],"expectedInheriting":row["rightsInheriting"]})
            live.require(reply["body"]["applied"],"actual closeDescriptor refused",reply);record["closed"].append(reply)
        for selected,ref in refs.items():
            reply=client.request("variables",{"variablesReference":ref},success=False)
            live.require("body" not in reply,"old scope survived WASIp1 edit",reply);record["mutation_stale"][selected]=reply
        current=live.actual_location(client);refs=scopes(client,current["thread"])
        expected={selected:custom(client,selected) for selected in TITLES}
        live.require(len(expected["args"])==160 and expected["args"][0]["value"]=='"wasip1\\x20scope"',"actual escaped guest argument",expected["args"][:1])
        live.require(len(expected["env"])==160 and {v["value"] for v in expected["env"]}=={f'"E{i:03}=value{i:03}"' for i in range(160)},"actual isolated guest environment")
        live.require(len(expected["preopens"])==140 and len(expected["fds"])==143,"actual sparse guest descriptor cardinality",{k:len(v) for k,v in expected.items()})
        live.require(expected["fds"][-1]["descriptor"]+1>len(expected["fds"]),"actual guest descriptor holes missing")
        record["expected"]=expected
        for selected in TITLES:
            ref=refs[selected]; total=len(expected[selected])
            for first,count in ((0,80),(63,80),(129,8),(total-1,64),(total,4),((1<<64)-1,1)):
                reply=client.request("variables",{"variablesReference":ref,"start":first,"count":count,"filter":"indexed"})
                verify(reply["body"]["variables"],expected[selected][first:first+count],args.out)
                record["pages"].append({"selection":selected,"start":first,"count":count,"response":reply})
            for options in ({},{"start":total-3,"count":0}):
                reply=client.request("variables",{"variablesReference":ref,**options})
                verify(reply["body"]["variables"],expected[selected][options.get("start",0):],args.out)
                if not options: record["all_scopes"][selected]=reply
            before=len(broker.commands);reply=client.request("variables",{"variablesReference":ref,"filter":"named"})
            live.require(reply["body"]["variables"]==[] and all(r["command"]=="status" for r in broker.commands[before:]),"named filter borrowed WASIp1 environment")
        record["external_step"]=broker.request(f"step wasm {current['thread']}")
        for selected,ref in refs.items():
            refusal=client.request("variables",{"variablesReference":ref},success=False)
            live.require("body" not in refusal,"retired WASIp1 scope published DATA",refusal);record["stale"][selected]=refusal
        for selected in TITLES:
            current=recurring_stop(client,function,target);refs=scopes(client,current["thread"]);copied=0
            def advance():
                nonlocal copied
                copied+=1
                if copied==2:
                    broker.after_page=None;record["mid_copy"][selected]={"external_step":broker.request(f"step wasm {current['thread']}")};return True
                return False
            broker.after_page=advance
            refusal=client.request("variables",{"variablesReference":refs[selected],"start":63,"count":80},success=False)
            live.require(copied==2 and "body" not in refusal,"partial WASIp1 scope published across actual stop",refusal)
            record["mid_copy"][selected]["refusal"]=refusal
        current=recurring_stop(client,function,target);refs=scopes(client,current["thread"])
        for selected,ref in refs.items():
            reply=client.request("variables",{"variablesReference":ref,"start":129,"count":8})
            verify(reply["body"]["variables"],expected[selected][129:137],args.out)
        client.evaluate(f"set wasm global 0 0 {current['thread']} bits i32 0");result=client.evaluate("continue")
        for _ in range(20):
            if b"guest exited: 0" in result: break
            result=client.evaluate("wait")
        live.require(b"guest exited: 0" in result,"actual WASIp1 fixture natural logical exit",result)
        record.update(passed=True,actual_guest_exit=result.decode(),totals={s:len(v) for s,v in expected.items()})
    finally:
        if "client" in locals(): record["requests"]=client.requests
        if broker is not None: record["broker_commands"]=broker.commands;broker.close()
        try:
            if record["passed"]: server.child.wait(timeout=10)
        finally:
            try: record["cleanup"]=server.close()
            finally:
                record["pins_after"]={n:live.sha(Path(n)) for n in pins};live.require(record["pins_after"]==pins,"immutable WASIp1 scope inputs changed")
                (args.out/"wasip1-scope-pages.json").write_text(json.dumps(record,indent=2)+"\n")
    print(f"run_dap_wasip1_scope_pages: PASS policy={args.policy} four-scopes>64 sparse-fds stale=12 natural-exit=0")
    return 0


if __name__ == "__main__": raise SystemExit(main())

#!/usr/bin/env python3
"""Build a small managed-general argv plan from real artifact pins; never run a compiler or guest."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
from generate_general_gc import FAMILIES, PHASES, ROOTS, oracle

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def pin(item):
    if type(item) is not dict or set(item) != {"path","sha256"}:
        raise ValueError("each actual artifact pin must have exactly path/sha256")
    path=Path(item["path"]).resolve(strict=True)
    if not path.is_file() or sha(path) != item["sha256"]:
        raise ValueError("actual artifact missing/changed: "+str(path))
    return {"path":str(path),"bytes":path.stat().st_size,"sha256":item["sha256"]}

def atomic_new(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    fd,tmp=tempfile.mkstemp(dir=path.parent,prefix=".managed-plan-")
    try:
        with os.fdopen(fd,"wb") as f:f.write(data);f.flush();os.fsync(f.fileno())
        os.link(tmp,path)
    finally:os.unlink(tmp)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--bindings",required=True,type=Path)
    p.add_argument("--out",required=True,type=Path)
    p.add_argument("--iterations",type=int,default=65536)
    p.add_argument("--warmup-iterations",type=int,default=250000)
    p.add_argument("--warmup-rounds",type=int,default=0)
    p.add_argument("--gc-telemetry",action="store_true")
    args=p.parse_args()
    if not 0<=args.warmup_rounds<=64: p.error("warmup budget")
    raw=json.loads(args.bindings.read_text())
    required={"schema","openjdk","graalvm","dotnet","java_class_dir","java_artifacts","dotnet_artifacts","compile_receipts","version_receipts"}
    if type(raw) is not dict or set(raw)!=required or raw["schema"]!="uwvm-managed-general-artifacts-v1":
        p.error("actual artifact binding shape")
    tools={k:pin(raw[k]) for k in ("openjdk","graalvm","dotnet")}
    artifacts={}
    for key in ("java_artifacts","dotnet_artifacts","compile_receipts","version_receipts"):
        if type(raw[key]) is not list or not raw[key]:p.error("missing actual "+key)
        artifacts[key]=[pin(x) for x in raw[key]]
        if len({x["path"] for x in artifacts[key]})!=len(artifacts[key]):p.error("duplicate artifact")
    java_dir=Path(raw["java_class_dir"]).resolve(strict=True)
    java_main=java_dir/"GeneralGc.class"
    if str(java_main) not in {x["path"] for x in artifacts["java_artifacts"]}:
        p.error("GeneralGc.class not bound")
    if java_main.read_bytes()[:8] != bytes.fromhex("cafebabe00000041"):
        p.error("require actual javac --release21 classfile 65.0")
    dll=[x for x in artifacts["dotnet_artifacts"] if Path(x["path"]).name=="GeneralGc.dll"]
    if len(dll)!=1:p.error("one actual GeneralGc.dll required")
    base=Path(__file__).parent
    source_paths=("managed_general/java/GeneralGc.java","managed_general/dotnet/Program.cs",
                  "managed_general/dotnet/GeneralGc.csproj","generate_general_gc.py",
                  "check_managed_general.py","prepare_managed_general_plan.py")
    sources={rel:{"bytes":(base/rel).stat().st_size,"sha256":sha(base/rel)} for rel in source_paths}
    java_common=["-Xms256m","-Xmx1g","-XX:+UseG1GC","-cp",str(java_dir),"GeneralGc"]
    engines={"openjdk-g1":[tools["openjdk"]["path"],*java_common],
             "graalvm-ce-g1":[tools["graalvm"]["path"],*java_common],
             "dotnet-workstation":[tools["dotnet"]["path"],dll[0]["path"]]}
    env={"JAVA_TOOL_OPTIONS":"","JDK_JAVA_OPTIONS":"","LD_PRELOAD":"",
         "DOTNET_TieredPGO":"1","DOTNET_gcServer":"0","DOTNET_GCHeapHardLimit":"0x40000000",
         "DOTNET_CLI_TELEMETRY_OPTOUT":"1","DOTNET_SYSTEM_GLOBALIZATION_INVARIANT":"1",
         "DOTNET_ROOT":str(Path(tools["dotnet"]["path"]).parent)}
    rows=[]
    for family in FAMILIES:
        for phase in PHASES:
            expected=oracle(family,phase,args.iterations)
            warm=oracle(family,phase,args.warmup_iterations)
            extra=[family,phase,str(args.iterations),str(args.warmup_iterations),str(args.warmup_rounds)]
            for values in (expected,warm):
                extra += [str(values[k]) for k in ("step_checksum_u32","root_checksum_u32","last_lcg_u32")]
            if args.gc_telemetry:extra.append("--gc-telemetry")
            for engine,argv in engines.items():
                rows.append({"engine":engine,"family":family,"phase":phase,"argv":[*argv,*extra],
                             "environment_delta":env,"expected":expected,"warm_expected":warm,
                             "root_groups":ROOTS,"table_root_slots":ROOTS*(2 if family=="reference-array" else 1),
                             "native_semantic_pass":None,"collector_qualified":False,
                             "whole_process_multitid":True,"single_tid_hw_counter_qualified":False})
    plan={"schema":"uwvm-managed-general-argv-plan-v1","formal_acceptance":False,
          "scope":"source-equivalent managed loop; not same Wasm bytes or collector-only ROI",
          "execution_protocol":"keeper must bind current64GiB/swap0/P0 multi-TID reference guard before execution; JSON is not a launcher",
          "current_native_tls_product_binding":None,
          "artifact_binding_path":str(args.bindings.resolve()),"artifact_binding_sha256":sha(args.bindings),
          "tools":tools,"artifacts":artifacts,"source_pins":sources,
          "iterations":args.iterations,"warmup_rounds":args.warmup_rounds,"rows":rows,
          "temperature_policy":"observe only; never reject by temperature",
          "pending":"actual semantic/counters/runtime-lib/fulltree/compile provenance and frequency/noise receipts; version smoke not PASS"}
    atomic_new(args.out,(json.dumps(plan,indent=2)+"\n").encode())
    print(json.dumps({"plan":str(args.out),"sha256":sha(args.out),"rows":len(rows),"execution":"not launched"}))
if __name__=="__main__":main()

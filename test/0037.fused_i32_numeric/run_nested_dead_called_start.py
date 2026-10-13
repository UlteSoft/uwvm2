#!/usr/bin/env python3
"""Source-only recipe; execute only through the keeper's original Linux cgroup."""
import argparse, hashlib, json, resource, subprocess, sys
from pathlib import Path
def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output",type=Path)
    parser.add_argument("--uwvm",type=Path,required=True)
    parser.add_argument("--wat2wasm",type=Path,required=True)
    parser.add_argument("--wasmtime",type=Path)
    parser.add_argument("--ros",action="store_true")
    parser.add_argument("--configuration")
    parser.add_argument("--source-id",required=True)
    args=parser.parse_args()
    here=Path(__file__).resolve().parent; root=here.parents[1]
    subprocess.run(["bash",str(root/"tools/ci/require_wasm3_test_cgroup.sh")],check=True)
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    output=args.output.resolve(); output.mkdir(parents=True,exist_ok=False)
    source=here/"source-oracle-nested-dead-called.json"
    for row in json.loads(source.read_text())["files"]:
        path=here/row["path"]
        if path.stat().st_size!=row["size"] or digest(path)!=row["sha256"]:
            raise RuntimeError("called nested-dead fixture source changed")
    tools=[]
    for role,path in (("uwvm",args.uwvm),("wat2wasm",args.wat2wasm),("wasmtime",args.wasmtime)):
        if path is None: continue
        path=path.resolve(); actual=subprocess.run([str(path),"--version"],capture_output=True,timeout=30)
        log=output/(role+"-version.log"); log.write_bytes(actual.stdout+actual.stderr)
        if actual.returncode: raise RuntimeError(role+" actual version failed")
        tools.append(dict(role=role,path=str(path),sha256=digest(path),version_log_sha256=digest(log)))
    wasm=output/"nested-dead-called-start.wasm"
    conversion=subprocess.run([str(args.wat2wasm.resolve()),"--enable-gc","--enable-function-references",
        str(here/"fixtures/i32_numeric_nested_dead_called_start.wat"),"-o",str(wasm)],capture_output=True,timeout=30)
    (output/"wat2wasm-conversion.log").write_bytes(conversion.stdout+conversion.stderr)
    if conversion.returncode: raise RuntimeError("actual Core3 called-start conversion failed")
    sys.path.insert(0,str(root/"test/0014.llvm_jit"))
    from run_wasm3_multi_memory import configurations
    features=["-WFE-gc","-WFE-function-references"]
    configs=[]
    for label,base in configurations(args.uwvm.resolve(),args.ros):
        command=[x for x in base[:-1] if x!="-WFE-multi-memory"]
        configs.append((label,command+["-Rct","0",*features,"--run"]))
    configs.append(("validator",[str(args.uwvm.resolve()),"-m","validation",*features,"--run"]))
    if args.wasmtime:
        configs.append(("wasmtime",[str(args.wasmtime.resolve()),"-C","cache=n",
            "-W","gc=y","-W","function-references=y","--invoke","_start"]))
    if args.configuration: configs=[row for row in configs if row[0]==args.configuration]
    if not configs: raise RuntimeError("no configurations selected")
    rows=[]
    for label,command in configs:
        actual=subprocess.run(command+[str(wasm)],capture_output=True,timeout=90)
        log=output/(label+".log"); log.write_bytes(actual.stdout+actual.stderr)
        row=dict(configuration=label,command=command+[str(wasm)],returncode=actual.returncode,
            passed=actual.returncode==0,log_sha256=digest(log),wasm_sha256=digest(wasm),
            source_has_real_start_and_called_child=True,execution_config=label!="validator",
            dead_numeric_execution_claimed=False)
        rows.append(row); (output/"rows.json").write_text(json.dumps(rows,indent=2)+"\n")
        if not row["passed"]: raise RuntimeError(label+" actual called-start result failed")
    summary=dict(passed=True,source_id=args.source_id,tools=tools,rows=rows,
        fixture_sha256=digest(here/"fixtures/i32_numeric_nested_dead_called_start.wat"),
        runner_sha256=digest(__file__),source_oracle_sha256=digest(source),
        configurations_helper_sha256=digest(root/"test/0014.llvm_jit/run_wasm3_multi_memory.py"),
        called_child_runtime_case_passed=any(row["execution_config"] for row in rows),
        actual_fused_translation_qualified=False,dead_numeric_execution_claimed=False,
        actual_IR_or_object_absence_qualified=False,assembly_qualified=False,performance_qualified=False)
    (output/"summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(dict(passed=True,configurations=len(rows),IR=False,ASM=False,performance=False)))
if __name__=="__main__": main()

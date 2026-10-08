#!/usr/bin/env python3
"""Synthetic Python-only checks; no native compilation, VM, SSH or process launch."""
import hashlib, importlib.util, json, pathlib, tempfile
import run_managed_general_measurement as r
P=pathlib.Path
spec=importlib.util.spec_from_file_location("scalar",P(__file__).with_name("generate_general_gc.py"))
g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
checks=0
def yes(value):
    global checks
    assert value;checks+=1
def rejects(call):
    global checks
    try:call()
    except (RuntimeError,ValueError,KeyError,IndexError):checks+=1;return
    raise AssertionError("synthetic bad input accepted")
delta={"JAVA_TOOL_OPTIONS":"","JDK_JAVA_OPTIONS":"","LD_PRELOAD":"",
 "DOTNET_TieredPGO":"1","DOTNET_gcServer":"0","DOTNET_GCHeapHardLimit":"0x40000000",
 "DOTNET_CLI_TELEMETRY_OPTOUT":"1","DOTNET_SYSTEM_GLOBALIZATION_INVARIANT":"1","DOTNET_ROOT":"/actual/pinned/sdk"}
inherited={"_JAVA_OPTIONS":"-XX:+UseZGC","JAVA_TOOL_OPTIONS":"-javaagent:wrong",
 "JDK_JAVA_OPTIONS":"-Xmx32g","JDK_AOT_VM_OPTIONS":"-XX:AOTMode=create",
 "COMPlus_gcServer":"1","DOTNET_GCHeapHardLimit":"0xfffff","DOTNET_STARTUP_HOOKS":"wrong.dll",
 "DOTNET_ROLL_FORWARD":"LatestMajor","dotnet_gcServer":"1","CORECLR_ENABLE_PROFILING":"1",
 "COR_PROFILER":"wrong","GRAALVM_OPTIONS":"wrong","CLASSPATH":"unbound",
 "LD_PRELOAD":"wrong.so","LD_AUDIT":"wrong-audit","LD_LIBRARY_PATH":"/wrong",
 "PYTHONPATH":"/untrusted","SAFE_TEST":"kept"}
child,receipt=r.sanitized_environment(inherited,delta,"/pinned/loader")
yes(child["SAFE_TEST"]=="kept" and child["LD_LIBRARY_PATH"]=="/pinned/loader")
yes(receipt["inherited_relevant"]=={k:v for k,v in inherited.items() if k!="SAFE_TEST"})
yes("_JAVA_OPTIONS" not in child and not any(k.upper().startswith(("COMPLUS_","CORECLR_","COR_","GRAAL")) for k in child))
yes(child["DOTNET_GCHeapHardLimit"]=="0x40000000" and "PYTHONPATH" not in child and "DOTNET_STARTUP_HOOKS" not in child)
yes(receipt["child_environment_sha256"]==hashlib.sha256(r.canonical(child)).hexdigest())
rejects(lambda:r.sanitized_environment(inherited,dict(delta,DOTNET_gcServer="1"),""))
rejects(lambda:r.sanitized_environment(inherited,dict(delta,COMPlus_gcServer="0"),""))
rejects(lambda:r.sanitized_environment(inherited,dict(delta,DOTNET_ROOT="relative"),""))
with tempfile.TemporaryDirectory(prefix="managed-synthetic-") as temp:
 root=P(temp)
 for family in g.FAMILIES:
  for phase in g.PHASES:
   expected=g.oracle(family,phase,65536)
   row=dict(engine="openjdk-g1",family=family,phase=phase,argv=["fake-not-executed"],expected=expected)
   result=dict(runtime="java",family=family,phase=phase,iterations=65536,warmup_iterations=250000,
    warmup_rounds=0,execution_ns=1000000,root_groups=1024,table_root_slots=2048 if family=="reference-array" else 1024,
    collector_roi=False,planned_syntax_allocations=expected["guest_planned_allocations"])
   result.update({k:expected[k] for k in ("step_checksum_u32","root_checksum_u32","last_lcg_u32","return_checksum_u32")})
   log=root/"synthetic-not-actual.log";log.write_text(json.dumps(result)+"\n")
   yes(r.semantic(row,log,0)["passed"] and not r.semantic(row,log,0)["collector_qualified"])
   bad=dict(result,step_checksum_u32=result["step_checksum_u32"]^1);log.write_text(json.dumps(bad)+"\n")
   yes(not r.semantic(row,log,0)["passed"])
   log.write_text(json.dumps(result)+"\n"+json.dumps(result)+"\n")
   yes(not r.semantic(row,log,0)["passed"])
   log.write_text(json.dumps(dict(result,execution_ns=0))+"\n")
   yes(not r.semantic(row,log,0)["passed"])
   log.write_text(json.dumps(dict(result,iterations=True))+"\n")
   yes(not r.semantic(row,log,0)["passed"])
 # .NET actual spelling/type; synthetic input only.
 row=dict(row,engine="dotnet-workstation",argv=["fake-not-executed","--gc-telemetry"])
 result.update(runtime="dotnet",server_gc=False,execution_ns=12500.0,
   gc_gen0_count_delta=0,gc_gen1_count_delta=0,gc_gen2_count_delta=0,gc_total_pause_ns_delta=0,
   main_thread_allocated_bytes_delta=1234,approximate_process_allocated_bytes_delta=1234)
 log.write_text("GC_TELEMETRY_START\nGC_TELEMETRY_END\n"+json.dumps(result)+"\n")
 parsed=r.semantic(row,log,0);yes(parsed["passed"] and parsed["internal_execution_ns"]=="12500.0")
 log.write_text("GC_TELEMETRY_START\nGC_TELEMETRY_END\n"+json.dumps(dict(result,server_gc=True))+"\n")
 yes(not r.semantic(row,log,0)["passed"])
 yes(not r.semantic(row,log,1)["passed"])
 # Proc-like files are synthetic and never name a real process.
 proc=root/"999999991";task=proc/"task"/"999999992";task.mkdir(parents=True)
 def status(uid="1000 1000 1000 1000",cpu="0",tgid="999999991"):
  return "Name: fake\nPid: 999999992\nTgid: "+tgid+"\nUid: "+uid+"\nCpus_allowed_list: "+cpu+"\n"
 parts=["R"]+["0"]*51;parts[19]="12345";stat="999999992 (synthetic) "+" ".join(parts)
 (task/"status").write_text(status());(task/"stat").write_text(stat);(task/"cgroup").write_text("0::/exact\n")
 observations,unknown,seen=r.tid_snapshot(proc,"0::/exact\n","0")
 yes(len(observations)==1 and not unknown and "999999992:12345" in seen)
 for field,value in (("cpu","0,2"),("uid","1000 0 1000 1000"),("tgid","999999990")):
  (task/"status").write_text(status(**{field:value}));rejects(lambda:r.tid_snapshot(proc,"0::/exact\n","0"))
 (task/"status").write_text(status());(task/"cgroup").write_text("0::/wrong\n")
 rejects(lambda:r.tid_snapshot(proc,"0::/exact\n","0"))
 (task/"cgroup").write_text("0::/exact\n");(task/"stat").unlink()
 observations,unknown,seen=r.tid_snapshot(proc,"0::/exact\n","0")
 yes(not observations and len(unknown)==1 and unknown[0]["second_task_stat_absent"] is True)
 try:r.tid_snapshot(proc,"0::/wrong\n","0")
 except RuntimeError as error:yes(error.managed_tid_observation["cgroup"]=="0::/exact\n")
 else:raise AssertionError("observed cgroup mismatch lost before vanished stat")
print(json.dumps({"scope":"synthetic Python-only parser/environment/TID checks","checks":checks,
                 "native_execution":False,"runtime_measurement_qualified":False}))

#!/usr/bin/env python3
"""Node fixed cold/warm whole-process derivative; sole keeper executes after review.

No compiler, download, SSH, administration, profiler or hardware counter path.
Frozen source-equivalent ports/old runners remain unchanged.
"""
import argparse, collections, decimal, hashlib, json, os, pathlib, re, resource, select
import signal, tempfile, threading, time, types
P = pathlib.Path
HOST_SHA = "673d961bb4fd37bd6ed98cc6c486cac3d2dc7fe256230795468ea58db7b42cd5"
HW_SHA = "266a56b3389ca5e120a757bd2a7a9b101b834810e5ba53f053b50f536aa425f8"
BASE_SHA = "0ab44317fea5c7cddcf236d79be232e24237386d17795684fa2f0fa51c487f89"
GENERAL_SHA = "91ee4854dcf7f176427f5943895bd9f933a5bfa2819c263c30e78774c8beada8"
GENERATOR_SHA = "eb6048d1d1fe7ad0f3264af41cc742b5a89ee17c2ca720c891ac5571fef9471e"
PORT_PINS = {'managed_general/node/GeneralGc.mjs':'2967db94f10bcc211d5b0bf1448b44a7fa765be299560b5fba672ab8913102c2'}
PARENT_SOURCE_SHA = '23f58b55dcf8bf4e360c7e67ce8bba9d1383872694331b878f57b3daf607feeb'
NODE_FLAGS = ['--max-old-space-size=1024','--max-semi-space-size=16']
JAVA_KEYS = {"_JAVA_OPTIONS","JAVA_TOOL_OPTIONS","JDK_JAVA_OPTIONS","JDK_AOT_VM_OPTIONS",
             "JAVA_HOME","JRE_HOME","CLASSPATH"}
LOADER_KEYS = {"LD_LIBRARY_PATH","LD_PRELOAD","LD_AUDIT","LD_DEBUG","LD_PROFILE",
               "LD_PROFILE_OUTPUT","GLIBC_TUNABLES"}
PYTHON_KEYS = {"PYTHONHOME","PYTHONPATH","PYTHONSTARTUP","PYTHONINSPECT","PYTHONDONTWRITEBYTECODE"}
PREFIXES = ("DOTNET_","COMPLUS_","GRAAL_","GRAALVM_","CORECLR_","COR_","NODE_","NPM_CONFIG_","V8_")
NODE_KEYS = {"UV_THREADPOOL_SIZE","UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT"}

def require(value, message):
    if not value: raise RuntimeError(message)

def digest(path):
    with P(path).open("rb") as stream: return hashlib.file_digest(stream,"sha256").hexdigest()

def canonical(value):
    return json.dumps(value,sort_keys=True,separators=(",",":"),allow_nan=False).encode()

def save(path, value):
    """Private atomic evidence write, including inherited configuration."""
    path=P(path); data=(json.dumps(value,indent=2,allow_nan=False)+"\n").encode()
    fd,tmp=tempfile.mkstemp(dir=path.parent,prefix=".managed-")
    try:
        with os.fdopen(fd,"wb") as stream: stream.write(data);stream.flush();os.fsync(stream.fileno())
        os.replace(tmp,path)
    finally:
        if os.path.exists(tmp): os.unlink(tmp)

def verify_pin(value):
    require(type(value) is dict and type(value.get("path")) is str and P(value["path"]).is_absolute()
            and type(value.get("sha256")) is str and re.fullmatch("[0-9a-f]{64}",value["sha256"]),
            "Missing actual pin")
    path=P(value["path"]); require(path.is_file() and digest(path)==value["sha256"],"Changed actual pin: "+str(path))
    if "bytes" in value:
        require(type(value["bytes"]) is int and path.stat().st_size==value["bytes"],"Changed actual size")

def relevant_key(key):
    return key in JAVA_KEYS|LOADER_KEYS|PYTHON_KEYS|NODE_KEYS or key.upper().startswith(PREFIXES)

def sanitized_environment(inherited, delta, loader):
    expected={"NODE_OPTIONS":"","NODE_DISABLE_COMPILE_CACHE":"1","PYTHONDONTWRITEBYTECODE":"1"}
    require(delta==expected,"Changed exact Node launch profile")
    require(type(loader) is str,"Missing actual loader path")
    captured={k:v for k,v in inherited.items() if relevant_key(k)}
    child={k:v for k,v in inherited.items() if not relevant_key(k)}
    child.update(delta)
    if loader: child["LD_LIBRARY_PATH"]=loader
    else: child.pop("LD_LIBRARY_PATH",None)
    return child,dict(inherited_relevant=captured,
        java_keys_absent_in_parent=sorted(JAVA_KEYS-set(inherited)),removed_names=sorted(captured),
        effective_relevant={k:v for k,v in child.items() if relevant_key(k)},
        child_environment_sha256=hashlib.sha256(canonical(child)).hexdigest(),
        effective_semantics="Requested launch settings; not an inferred runtime effective-settings report")

def sdk_tree(root):
    root=P(root).resolve(strict=True); require(root.is_dir(),"SDK root missing")
    entries=[]
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            resolved=path.resolve(strict=True)
            require(resolved.is_relative_to(root),"SDK symlink escapes bound root")
            entries.append(dict(relative=path.relative_to(root).as_posix(),symlink=os.readlink(path),
                                resolved_relative=resolved.relative_to(root).as_posix()))
        elif path.is_file():
            entries.append(dict(relative=path.relative_to(root).as_posix(),bytes=path.stat().st_size,sha256=digest(path)))
    require(entries,"Empty actual SDK tree")
    return dict(path=str(root),tree_sha256=hashlib.sha256(canonical(entries)).hexdigest(),entries=entries)

def load_source(here, name, sha):
    path=here/name; require(digest(path)==sha,"Frozen dependency changed: "+name)
    mod=types.ModuleType("managed_frozen_"+path.stem); mod.__file__=str(path)
    exec(compile(path.read_bytes(),str(path),"exec"),mod.__dict__)
    return mod

def validate_plan(plan,here,oracle):
    require(plan.get("schema")=="uwvm-node-general-argv-plan-v1" and plan.get("mode") in ("cold","warm") and
            plan.get("current_native_tls_product_binding") is None,"Only fixed Node source plan supported")
    require(digest(here/"run_managed_general_measurement.py")==PARENT_SOURCE_SHA,"Unchanged parent protocol source required")
    for key in ("node","version_receipt","syntax_receipt"):verify_pin(plan[key])
    sources=set(PORT_PINS)|{"generate_general_gc.py","prepare_node_general_plan.py","run_node_general_measurement.py"}
    require(set(plan["source_pins"])==sources,"Missing/extra Node source pins")
    for rel,pin in plan["source_pins"].items():
        require(type(pin)==dict and set(pin)=={"bytes","sha256"} and (here/rel).stat().st_size==pin["bytes"] and digest(here/rel)==pin["sha256"],"Actual Node source changed")
    for rel,sha in PORT_PINS.items():require(digest(here/rel)==sha,"Frozen Node port changed")
    binding=P(plan["artifact_binding_path"]); require(digest(binding)==plan["artifact_binding_sha256"],"Actual Node binding changed")
    raw=json.loads(binding.read_bytes())
    require(set(raw)=={"schema","node","version_receipt","syntax_receipt"} and raw["schema"]=="uwvm-node-general-artifacts-v1","Actual artifact binding shape")
    for key in ("node","version_receipt","syntax_receipt"):
        require(raw[key]=={k:plan[key][k] for k in ("path","sha256")},"Node actual artifact binding differs")
    version=json.loads(P(plan["version_receipt"]["path"]).read_bytes())
    require(version["returncode"]==0 and version["version"]==plan["actual_version"] and
            version["argv"]==[plan["node"]["path"],"--eval","console.log(JSON.stringify({node:process.version,v8:process.versions.v8,platform:process.platform,arch:process.arch}))"],
            "Actual version receipt changed")
    require(plan["actual_version"]["node"]=="v26.10.0" and plan["actual_version"]["platform"]=="linux" and
            plan["actual_version"]["arch"]=="x64" and type(plan["actual_version"]["v8"]) is str and
            re.fullmatch(r"14\.6\.202\.34(?:-.+)?",plan["actual_version"]["v8"]) is not None,"Actual target runtime version changed")
    source=str((here/"managed_general/node/GeneralGc.mjs").resolve(strict=True))
    syntax=json.loads(P(plan["syntax_receipt"]["path"]).read_bytes())
    require(syntax=={"argv":[plan["node"]["path"],"--check",source],"returncode":0,
                   "node_sha256":plan["node"]["sha256"],"source_sha256":PORT_PINS["managed_general/node/GeneralGc.mjs"]},"Actual Node syntax receipt differs")
    wanted={(f,p,65536) for f in oracle.FAMILIES for p in oracle.PHASES} if plan["mode"]=="cold" else {
        (f,p,n) for f,p in (("reference-array","allocate"),("mutable-struct","mutate")) for n in (1000000,2000000)}
    require(type(plan["rows"]) is list and len(plan["rows"])==len(wanted),"Exact cell count required")
    cells=set();rounds=0 if plan["mode"]=="cold" else 8
    for row in plan["rows"]:
        cell=(row["family"],row["phase"],row["iterations"]);require(cell in wanted and cell not in cells,"Unknown/duplicate Node cell");cells.add(cell)
        require(row["engine"]=="node-v8" and type(row["iterations"]) is int and type(row["warmup_rounds"]) is int and row["warmup_rounds"]==rounds,"Actual Node profile changed")
        expected=oracle.oracle(row["family"],row["phase"],row["iterations"]);warm=oracle.oracle(row["family"],row["phase"],250000)
        require(row["expected"]==expected and row["warm_expected"]==warm and row["actual_version"]==plan["actual_version"],"Independent prelaunch scalar expectation/version changed")
        extra=[row["family"],row["phase"],str(row["iterations"]),"250000",str(rounds)]
        for values in (expected,warm):extra.extend(str(values[k]) for k in ("step_checksum_u32","root_checksum_u32","last_lcg_u32"))
        if row["argv"][-1]=="--gc-telemetry":extra.append("--gc-telemetry")
        require(row["argv"]==[plan["node"]["path"],*NODE_FLAGS,source,*extra],"Actual Node argv changed")
        require(row["whole_process_multitid"] is True and row["single_tid_hw_counter_qualified"] is False and row["collector_qualified"] is False,"Node must not claim collector/singleTID counter permission")
        sanitized_environment({},row["environment_delta"],"")
    return raw

def close(plan, here, oracle, closure, out, stage):
    validate_plan(plan,here,oracle)
    require(closure.get("schema")=="uwvm-node-general-runtime-closure-v1" and
            set(closure["sdk_roots"])=={"node"} and type(closure["loader_environment"]) is dict and
            set(closure["loader_environment"])=={"LD_LIBRARY_PATH"} and type(closure["files"]) is list and closure["files"],"Actual SDK/loader closure shape")
    for value in closure["files"]: verify_pin(value)
    trees={}
    for engine,value in closure["sdk_roots"].items():
        require(set(value)=={"path","tree_sha256"} and type(value["path"]) is str and P(value["path"]).is_absolute() and type(value["tree_sha256"]) is str and re.fullmatch("[0-9a-f]{64}",value["tree_sha256"]),"Actual SDK pin shape")
        tree=sdk_tree(value["path"]); require(tree["tree_sha256"]==value["tree_sha256"],"Actual SDK tree changed")
        require(P(plan[engine]["path"]).resolve(strict=True).is_relative_to(P(tree["path"])),"Actual runtime outside SDK closure")
        trees[engine]=tree
    save(out/("sdk-"+stage+".json"),trees)
    return {k:v["tree_sha256"] for k,v in trees.items()}

def semantic(row, log, code):
    result=dict(passed=False,failures=[],collector_qualified=False,single_tid_hw_counter_qualified=False)
    lines=log.read_text(errors="replace").splitlines()
    try:
        objects=[json.loads(line,parse_float=decimal.Decimal) for line in lines if line.startswith("{")]
        require(len(objects)==1 and type(objects[0]) is dict,"Missing/duplicate actual result JSON")
        actual=objects[0]; expected=row["expected"]
        wanted={"runtime":"node","family":row["family"],"phase":row["phase"],
            "iterations":row["iterations"],"warmup_iterations":250000,"warmup_rounds":row["warmup_rounds"],
            "root_groups":1024,"table_root_slots":2048 if row["family"]=="reference-array" else 1024,
            "collector_roi":False,"planned_syntax_allocations":expected["guest_planned_allocations"],
            "physical_object_count_known":False,"gc_reclaimed_objects":None,"gc_collection_count":None,
            "numeric_array_representation":"Uint32Array + managed ArrayBuffer/backing storage"}
        wanted.update({k:expected[k] for k in ("step_checksum_u32","root_checksum_u32","last_lcg_u32","return_checksum_u32")})
        wanted.update(row["actual_version"])
        wanted["node_version"]=wanted.pop("node");wanted["v8_version"]=wanted.pop("v8")
        for key,value in wanted.items():require(key in actual and type(actual[key]) is type(value) and actual[key]==value,"Actual scalar/config mismatch: "+key)
        require(actual.get("actual_exec_argv")==NODE_FLAGS,"Actual Node VM flags differ")
        duration=actual.get("execution_ns")
        require(type(duration) is int and 0<duration<2**53,"Missing/nonfinite/nonintegral internal execution_ns")
        require(type(actual.get("actual_heap_size_limit")) is int and actual["actual_heap_size_limit"]>0,"Missing actual V8 heap limit")
        require(code==0,"Actual Node guest failed")
        if row["argv"][-1]=="--gc-telemetry":
            require(lines.count("GC_TELEMETRY_START")==lines.count("GC_TELEMETRY_END")==1,"Telemetry markers ambiguous")
            require(actual.get("memory_snapshot_is_allocated_byte_counter") is False,"Heap snapshots cannot be allocation counts")
            for key in ("memory_snapshot_before","memory_snapshot_after"):
                snap=actual.get(key);require(type(snap) is dict and set(snap)=={"rss","heapTotal","heapUsed","external","arrayBuffers"} and all(type(v) is int and v>=0 for v in snap.values()),"Actual memory snapshot malformed")
        result.update(passed=True,internal_execution_ns=str(duration),
            actual_result={k:str(v) if isinstance(v,decimal.Decimal) else v for k,v in actual.items()},
            scope="Run setup/LCG/allocations/fields/final roots; excludes warmup/output; not collector-only")
    except (ValueError,RuntimeError,decimal.InvalidOperation) as error: result["failures"].append(str(error))
    return result

def tid_snapshot(proc, expected_cgroup, cpu, previous=None):
    """Actual observations only. A vanished worker is unknown coverage, never identity proof."""
    observations=[]; unknown=[]; seen=dict(previous or {})
    try: tasks=list((proc/"task").iterdir())
    except FileNotFoundError: return observations,[dict(reason="actual task directory missing",path=str(proc/"task"))],seen
    for task in tasks:
        tid=int(task.name); record=dict(tid=tid,observed_ns=time.monotonic_ns())
        try:
            status=(task/"status").read_text()
            fields=dict(line.split(":",1) for line in status.splitlines() if ":" in line)
            record.update(status_raw=status)
            require(list(map(int,fields["Uid"].split()))==[1000]*4,"Observed managed TID wrong UID")
            require(fields["Cpus_allowed_list"].strip()==cpu,"Observed managed TID wrong CPU")
            require(int(fields["Tgid"])==int(proc.name) and int(fields["Pid"])==tid,"Observed managed TID wrong TGID/PID")
            cgroup=(task/"cgroup").read_text();record["cgroup"]=cgroup
            require(cgroup==expected_cgroup,"Observed managed TID wrong cgroup")
            stat=(task/"stat").read_text();record["stat_raw"]=stat
            parts=stat[stat.rfind(")")+2:].split();require(len(parts)>=20,"Incomplete managed TID stat")
            birth=int(parts[19]); record["birth"]=birth
            second=(task/"stat").read_text(); parts2=second[second.rfind(")")+2:].split()
            require(len(parts2)>=20,"Incomplete second managed TID stat")
            if int(parts2[19])!=birth:
                record.update(reason="actual TID birth changed between observations",second_stat_raw=second);unknown.append(record)
            else: observations.append(record);seen[str(tid)+":"+str(birth)]=True
        except FileNotFoundError as failure:
            record.update(reason="worker disappeared mid-observation",missing_path=str(failure.filename))
            try: record["second_task_stat_raw"]=(task/"stat").read_text()
            except FileNotFoundError: record["second_task_stat_absent"]=True
            # Retain the actual global TID/TGID if it now exists; never signal it.
            try: record["second_global_status_raw"]=(P("/proc")/str(tid)/"status").read_text()
            except FileNotFoundError: record["second_global_status_absent"]=True
            unknown.append(record)
        except (RuntimeError,KeyError,ValueError,IndexError) as failure:
            # Preserve the exact partial observation that caused rejection.
            # Missing later fields cannot erase an already observed mismatch.
            failure.managed_tid_observation=record
            raise
    return observations,unknown,seen

def guard_class(host, base, general, out, expected_environment):
    class ManagedGuard(host.HostGuard):
        def __init__(self, admission):
            self.lock=threading.RLock(); self.tids=collections.deque(maxlen=64); self.unknown_count=0;self.seen={}
            self.environment_witnesses={}; self.terminal_receipts=[]
            super().__init__(base,admission)
        def identity(self, entry):
            pid=entry["process"].pid
            row=base.ident(pid)
            if row["argv"] or row["state"]=="Z" or entry["actual_exec"] is None: return row
            stat=general.actual_mm_stat((P("/proc")/str(pid)/"stat").read_text())
            if not general.clear_mm_retirement_candidate(entry,row,stat,self.cg,os.getpid()):return row
            fd=entry["pidfd"]; deadline=time.monotonic_ns()+2_000_000_000
            receipt=dict(original_pid=pid,original_birth=entry["birth"],original_pidfd=fd,initial=base.json_ident(row),
                         initial_stat=stat,started_ns=time.monotonic_ns(),terminal_confirmed=False)
            while True:
                require(time.monotonic_ns()<=deadline and entry["pidfd"]==fd,"Original terminal observation timeout/FD change")
                ready=bool(select.select([fd],[],[],0)[0])
                try:
                    current=base.ident(pid);stat=general.actual_mm_stat((P("/proc")/str(pid)/"stat").read_text())
                    require(general.clear_mm_retirement_candidate(entry,current,stat,self.cg,os.getpid()),"Actual cleared-MM identity/MM changed")
                    if current["state"]==stat["state"]=="Z" and bool(select.select([fd],[],[],0)[0]):
                        receipt.update(terminal_confirmed=True,finished_ns=time.monotonic_ns(),actual=base.json_ident(current),actual_stat=stat)
                        self.terminal_receipts.append(receipt);return current
                except FileNotFoundError:
                    if ready and not (P("/proc")/str(pid)/"stat").exists():
                        receipt.update(terminal_confirmed=True,finished_ns=time.monotonic_ns(),actual_stat_absent=True)
                        self.terminal_receipts.append(receipt);raise
                time.sleep(.005)
        def check(self):
            with self.lock:
                require(P("/proc/sys/kernel/random/boot_id").read_text().strip()==self.admission["boot_id"],"Boot changed")
                snapshot={k:(self.root/k).read_text().strip() for k in ("memory.max","memory.swap.max","cpuset.cpus.effective","cgroup.procs")}
                host.validate_scope(snapshot,self.init["pid"],os.getpid(),[e["process"].pid for e in self.owned if not e["reaped"]])
                for initial in (self.self,self.init):
                    now=base.ident(initial["pid"])
                    require(all(now[k]==initial[k] for k in ("birth","ppid","pgid","uid","cgroup","argv","cpus")),"Host baseline identity changed")
                    exe=self.self_exe if initial is self.self else self.admission["host_init"]["exe"]
                    require(os.readlink(P("/proc")/str(initial["pid"])/"exe")==exe,"Host baseline executable changed")
                events=base.counts(self.root/"memory.events")
                require(all(events[k]==self.events[k] for k in ("oom","oom_kill")),"Target OOM changed")
                require(int((self.root/"memory.current").read_text())<56<<30,"Target cgroup headroom")
                fs=os.statvfs(self.work);require(fs.f_bavail*fs.f_frsize>=1<<30,"Diagnostic disk floor1GiB")
                available=next(l.split()[1] for l in P("/proc/meminfo").read_text().splitlines() if l.startswith("MemAvailable:"))
                require(int(available)*1024>=8<<30,"Physical memory headroom8GiB")
                for entry in self.owned:
                    if entry["reaped"]:continue
                    pid=entry["process"].pid
                    try:row=self.identity(entry)
                    except FileNotFoundError:
                        require(bool(select.select([entry["pidfd"]],[],[],0)[0]),"Missing original managed process not retired");continue
                    require(row["birth"]==entry["birth"] and row["ppid"]==os.getpid() and row["pgid"]==pid,"Managed original ancestry changed")
                    require(pid in set(map(int,snapshot["cgroup.procs"].split())) or row["state"]=="Z","Managed child escaped roster")
                    require(row["uid"]==[1000]*4 and row["cgroup"]==self.cg and row["cpus"]==entry["cpu"]=="0","Managed UID/CG/P0 changed")
                    require(row["rss"]<=6<<30,"Managed RSS above6GiB")
                    if row["state"]=="Z":continue
                    allowed=[entry["exec_argv"]] if entry["actual_exec"] is not None else entry["allowed_argv"]
                    require(row["argv"] in allowed,"Actual managed argv changed")
                    if row["argv"]==entry["exec_argv"]:
                        try:
                            loaded=P("/proc")/str(pid)/"exe"
                            require(os.readlink(loaded)==str(entry["expected_exe"]),"Actual loaded managed executable changed")
                            require(digest(loaded)==digest(entry["expected_exe"]),"Actual loaded managed ELF bytes changed")
                            if entry["actual_exec"] is None:
                                entry["actual_exec"]=dict(base.json_ident(row),executable=str(entry["expected_exe"]),time_ns=time.time_ns())
                            if pid not in self.environment_witnesses:
                                actual=(P("/proc")/str(pid)/"environ").read_bytes().split(b"\0")
                                pairs=[part.split(b"=",1) for part in actual if part]
                                require(all(len(v)==2 for v in pairs),"Malformed actual exec environment")
                                relevant={os.fsdecode(k):os.fsdecode(v) for k,v in pairs if relevant_key(os.fsdecode(k))}
                                require(len(pairs)==len({k for k,v in pairs}),"Duplicate actual environment keys")
                                require(relevant=={k:v for k,v in expected_environment.items() if relevant_key(k)},
                                        "Actual managed initial environment differs")
                                self.environment_witnesses[pid]=dict(observed_ns=time.monotonic_ns(),actual_relevant=relevant,
                                    actual_environ_sha256=hashlib.sha256(b"\0".join(actual)).hexdigest())
                        except FileNotFoundError:
                            require(bool(select.select([entry["pidfd"]],[],[],0)[0]),"Loaded exec/environment vanished before original retirement");continue
                    try: observations,unknown,self.seen=tid_snapshot(P("/proc")/str(pid),self.cg,"0",self.seen)
                    except (RuntimeError,KeyError,ValueError,IndexError) as failure:
                        save(out/("tid-rejection-"+str(time.monotonic_ns())+".json"),
                            dict(original_pid=pid,original_birth=entry["birth"],original_pidfd=entry["pidfd"],
                                 error=type(failure).__name__+": "+str(failure),
                                 actual_observation=getattr(failure,"managed_tid_observation",None)))
                        raise
                    self.unknown_count+=len(unknown)
                    self.tids.append(dict(pid=pid,original_birth=entry["birth"],observed_ns=time.monotonic_ns(),
                        actual_tids=observations,qualification_unknown=unknown))
    return ManagedGuard

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ("plan","admission","sdk-closure","out"):p.add_argument("--"+name,type=P,required=True)
    p.add_argument("--execute",action="store_true");args=p.parse_args()
    here=P(__file__).parent;require(not args.out.exists(),"Fresh evidence directory required")
    require(args.execute,"Only reviewed keeper --execute launches guests")
    args.out.mkdir(parents=True,mode=0o700); resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    def cancel(signum,frame):raise KeyboardInterrupt("Controlled cancellation "+str(signum))
    signal.signal(signal.SIGTERM,cancel)
    host=load_source(here,"run_current_host_pcore_hw_counting.py",HOST_SHA)
    hw=load_source(here,"run_current_pcore_hw_counting.py",HW_SHA)
    original=load_source(here,"run_current_pcore_diagnostic.py",BASE_SHA)
    general=load_source(here,"run_current_general_gc.py",GENERAL_SHA)
    oracle=load_source(here,"generate_general_gc.py",GENERATOR_SHA)
    plan=json.loads(args.plan.read_text()); admission=json.loads(args.admission.read_text()); sdk=json.loads(args.sdk_closure.read_text())
    validate_plan(plan,here,oracle)
    requested=plan["rows"][0]["environment_delta"]
    require(all(row["environment_delta"]==requested for row in plan["rows"]),"Per-row managed environment changed")
    environment,config=sanitized_environment(dict(os.environ),requested,sdk["loader_environment"]["LD_LIBRARY_PATH"])
    save(args.out/"environment.json",config);save(args.out/"plan.json",plan);save(args.out/"admission.json",admission);save(args.out/"runtime-closure.json",sdk)
    inputs={str(path):digest(path) for path in (P(__file__),args.plan,args.admission,args.sdk_closure,here/"run_managed_general_measurement.py")}
    require(digest(admission["trusted_wrapper"]["path"])==admission["trusted_wrapper"]["sha256"],"Actual admission wrapper changed")
    inputs[admission["trusted_wrapper"]["path"]]=admission["trusted_wrapper"]["sha256"]
    for filename,sha in (("run_current_host_pcore_hw_counting.py",HOST_SHA),("run_current_pcore_hw_counting.py",HW_SHA),
                         ("run_current_pcore_diagnostic.py",BASE_SHA),("run_current_general_gc.py",GENERAL_SHA),("generate_general_gc.py",GENERATOR_SHA)):
        inputs[str(here/filename)]=sha
    guard=guard_class(host,original,general,args.out,environment)(admission)
    adapted=host.HostBase(original,guard.root); base=types.SimpleNamespace(**original.__dict__)
    base.telemetry=adapted.telemetry
    def popen(*a,**kw):
        require("env" not in kw,"Unexpected bootstrap env override")
        return hw.subprocess.Popen(*a,env=environment,**kw)
    spawn=types.FunctionType(hw.spawn_stopped.__code__,dict(hw.spawn_stopped.__globals__,
        subprocess=types.SimpleNamespace(**dict(hw.subprocess.__dict__,Popen=popen))),"managed_spawn_stopped")
    protocol=types.SimpleNamespace(spawn_stopped=spawn,signal_owned=hw.signal_owned)
    sample=types.FunctionType(general.plain_sample.__code__,dict(general.plain_sample.__globals__,
        plain_role=lambda _: "reference",save=save),"managed_plain_sample")
    rows=[];before=after=complete=False;error=None;trees=None
    try:
        trees=close(plan,here,oracle,sdk,args.out,"before");guard.check();before=True
        for index,row in enumerate(plan["rows"]):
            out=args.out/(str(index)+"-"+row["engine"]+"-"+row["family"]+"-"+row["phase"]);out.mkdir(mode=0o700)
            item=dict(row,profile=row["engine"],fixture=row["family"]+"-"+row["phase"],
                      argv=["taskset","-c","0",*row["argv"]])
            unknown_before=guard.unknown_count; seen_before=set(guard.seen);terminal_before=len(guard.terminal_receipts)
            measured=sample(base,protocol,guard,item,out,lambda item,log,code:semantic(row,log,code))
            pid=measured["guest_admission"]["pid"] if measured["guest_admission"] else None
            measured.update(measurement_family="node-unprofiled-whole-process",
                collector_qualified=False,single_tid_hw_counter_qualified=False,host_smt_noise="unknown without actual observer",
                actual_tid_births_observed=sorted(set(guard.seen)-seen_before),
                tid_coverage_unknown_count=guard.unknown_count-unknown_before,
                actual_exec_environment=guard.environment_witnesses.get(pid),
                managed_terminal_receipts=guard.terminal_receipts[terminal_before:],
                temperature_policy="observation_only")
            save(out/"tid-observations.json",list(guard.tids));guard.tids.clear()
            if measured["tid_coverage_unknown_count"]:measured["quality_failures"].append("Actual worker retirement gaps; TID coverage unknown")
            if measured["actual_exec_environment"] is None:measured["quality_failures"].append("Initial exec environment not observed; qualification unknown")
            measured["sample_quality_passed"]=not measured["hard_failures"] and not measured["quality_failures"] and measured["semantic_receipt"]["passed"]
            save(out/"qualified.json",measured);rows.append(measured)
            require(not measured["hard_failures"],"Actual ownership/resource/deadline failure")
            require(measured["semantic_receipt"]["passed"],"Actual managed scalar/telemetry/exit failure")
        require(close(plan,here,oracle,sdk,args.out,"after")==trees,"Actual SDK closure changed")
        require(all(digest(path)==sha for path,sha in inputs.items()),"Immutable measurement inputs changed")
        guard.check();after=complete=True
    except BaseException as failure:error=type(failure).__name__+": "+str(failure);raise
    finally:
        save(args.out/"summary.json",dict(schema="uwvm-node-general-plain-measurement-v1",mode=plan["mode"],complete=complete,
            closure_before_ok=before,closure_after_ok=after,error=error,input_sha256=inputs,
            rows=rows,semantic_pass_count=sum(row["semantic_receipt"]["passed"] for row in rows),
            semantic_and_closure_passed=complete and before and after and len(rows)==len(plan["rows"]) and
                all(not row["hard_failures"] and row["semantic_receipt"]["passed"] for row in rows),
            measurement_passed=complete and before and after and len(rows)==len(plan["rows"]) and all(row["sample_quality_passed"] for row in rows),
            qualified_sample_count=sum(row["sample_quality_passed"] for row in rows) if complete and before and after else 0,
            contemporaneous_runtime_dso_maps_captured=False,
            formal_acceptance=False,collector_qualified=False,single_tid_hw_counter_qualified=False,
            temperature_policy="observation_only",limitations=["Whole-process wait4 includes all managed VM TIDs/startup/JIT/output.",
                "Internal timer excludes warmup/output and includes setup/checksum, not collector-only.",
                "Cold65536 semantics versus warmed1M/2M are distinct; no steady-tier/collector/ranking claim.",
                "scaling_cur_freq samples are snapshots, not ROI effective GHz; no PMU/Vtune frequency inferred.",
                "No hardware/profiler invocation and no widening of frozen product single-TID counters."]))
if __name__=="__main__": main()

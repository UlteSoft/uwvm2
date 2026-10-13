#!/usr/bin/env python3
"""Execute and inspect FastIO termination; source-bound component evidence only."""
from pathlib import Path
import hashlib,json,os,re,shlex,signal,subprocess,sys,time
from elf_function_extent import function_extent
r=Path(__file__).parent
config_path=Path(sys.argv[1]); c=json.loads(config_path.read_text()); out=Path(c["output"])
sha=lambda p:hashlib.file_digest(Path(p).open("rb"),"sha256").hexdigest()
def verify(pins):
 for p,h in pins.items():assert sha(p)==h,("changed input",p)
def run(argv,log):
 start=time.monotonic()
 p=subprocess.run(argv,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
 log.write_bytes(p.stdout)
 return dict(argv=argv,exit=p.returncode,seconds=time.monotonic()-start,log_sha256=sha(log))
assert Path("/proc/self/cgroup").read_text().strip()==c["cgroup"]
subprocess.run(["/usr/bin/bash",c["cgroup_guard"]],check=True)
verify(c["pins"]); rows=[]
for item in c["items"]:
 folder=out/item["name"];folder.mkdir()
 obj=folder/"probe.o"; exe=folder/"probe"; dep=folder/"probe.d"
 compile_argv=item["flags"]+["-MD","-MF",str(dep),"-c",c["test"],"-o",str(obj)]
 compilation=run(compile_argv,folder/"compile.log");assert compilation["exit"]==0,compilation
 paths=shlex.split(dep.read_text().replace("\\\n"," ").partition(":")[2])
 actual={str(Path(p).resolve()):sha(p) for p in paths}
 assert all(c["pins"].get(p)==h for p,h in actual.items()),("unbound actual dependency",set(actual)-set(c["pins"]))
 link=run(item["flags"]+[str(obj),"-o",str(exe)],folder/"link.log");assert link["exit"]==0,link
 section,start,size=function_extent(obj.read_bytes(),"uwvm2_test_fast_terminate")
 argv=c["objdump_prefix"]+[c["objdump"],"--no-show-raw-insn","--disassemble-symbols=uwvm2_test_fast_terminate",str(obj)]
 dis=run(argv,folder/"assembly.log");assert dis["exit"]==0,dis
 text=(folder/"assembly.log").read_text()
 ins=[]
 for line in text.splitlines():
  m=re.match(r"^\s*([0-9a-fA-F]+):\s+(\S+)(.*)$",line)
  if m and start<=int(m[1],16)<start+size:ins.append((m[2],m[3].strip()))
 assert size==4 and len(ins)==1,(size,ins)
 if item["builtin_control"]:
  assert ins==[("ud", "0")] or (ins[0][0]=="amswap.w" and ins[0][1]=="$zero, $ra, $zero"),ins
 else:assert ins==[("break","0")],ins
 execution=run(c["qemu_prefix"]+[str(exe)],folder/"run.log")
 expected=-signal.SIGSEGV if item["builtin_control"] else -signal.SIGTRAP
 assert execution["exit"]==expected,execution
 row=dict(name=item["name"],builtin_control=item["builtin_control"],passed=True,
  product_sha256=sha(exe),object_sha256=sha(obj),function_section=section,
  function_size=size,instructions=ins,compile=compilation,link=link,
  assembly=dis,execution=execution,actual_dependency_pins=actual)
 rows.append(row);(out/"checkpoint.json").write_text(json.dumps(rows,indent=2)+"\n")
 print("PASS",item["name"],"signal",-execution["exit"],"bytes",size,ins,flush=True)
verify(c["pins"])
(out/"qualification.json").write_text(json.dumps(dict(passed=True,repo=c["repo"],
 source_identities=c["source_identities"],configuration_sha256=sha(config_path),rows=rows,
 fixed_termination_runs=sum(not x["builtin_control"] for x in rows),
 builtin_control_runs=sum(x["builtin_control"] for x in rows),
 scope="Actual FastIO component only: LoongArch scalar/LSX/LASX O1/O2/O3, exact one-instruction breakpoint and SIGTRAP; Clang builtin control proves deliberate null atomic causes SIGSEGV. No Wasm/JIT/tail/cache completion."),indent=2)+"\n")

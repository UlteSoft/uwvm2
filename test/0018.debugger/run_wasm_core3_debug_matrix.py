#!/usr/bin/env python3
"""Run fixture validation, Wasm stops, queries, instruction steps and trace.

This is a feature smoke matrix, not exhaustive validation or a concurrency proof.
"""
from pathlib import Path
import json,subprocess,selectors,os,time,re,traceback
import argparse, hashlib
from wasm_query_evidence import query_evidence
p=argparse.ArgumentParser(description="Real Core 3 Wasm debugger smoke matrix; no WASIp1 or source-language fixtures")
p.add_argument("--source-root",type=Path,required=True);p.add_argument("--binary",type=Path,required=True)
p.add_argument("--wasm-tools",type=Path,required=True);p.add_argument("--out",type=Path,required=True)
p.add_argument("--matrix",type=Path)
p.add_argument("--runner-prefix-json",type=Path,help="JSON argv prefix for QEMU user mode")
p.add_argument("--prompt-timeout",type=int,default=30)
p.add_argument("--stop-timeout",type=int,default=20)
a=p.parse_args();S=a.source_root.resolve();binary=a.binary.resolve();out=a.out;out.mkdir(parents=True,exist_ok=False)
assert 1 <= a.prompt_timeout <= 600 and 1 <= a.stop_timeout <= 120
runner=json.loads(a.runner_prefix_json.read_text()) if a.runner_prefix_json else []
assert isinstance(runner,list) and all(isinstance(x,str) and x for x in runner)
TD=out/"fixtures";TD.mkdir();matrix=a.matrix or S/"test/0018.debugger/fixtures/wasm_core3_debug_matrix.json";cases=json.loads(matrix.read_text());rows=[]
subprocess.run(["bash",str(S/"tools/ci/require_wasm3_test_cgroup.sh")],check=True)
for case in cases:
 wat=TD/(case["name"]+".wat");wasm=TD/(case["name"]+".wasm");wat.write_text(case["wat"]+"\n")
 subprocess.run([str(a.wasm_tools),"parse",str(wat),"-o",str(wasm)],check=True)
 subprocess.run([str(a.wasm_tools),"validate","--features","all",str(wasm)],check=True)
(out/"inputs.json").write_text(json.dumps({"binary_sha256":hashlib.sha256(binary.read_bytes()).hexdigest(),"matrix_sha256":hashlib.sha256(matrix.read_bytes()).hexdigest(),"harness_sha256":hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),"query_qualification_sha256":hashlib.sha256(Path(__file__).with_name("wasm_query_evidence.py").read_bytes()).hexdigest(),"runner_prefix":runner,"runner_sha256":hashlib.sha256(Path(runner[0]).read_bytes()).hexdigest() if runner else None,"cases":len(cases),"cgroup":Path("/proc/self/cgroup").read_text()},indent=2)+"\n")
def leb(b,p):
 n=0;s=0
 while True:
  v=b[p];p+=1;n|=(v&127)<<s
  if v<128:return n,p
  s+=7;assert s<70
def exported(path):
 b=path.read_bytes();p=8
 while p<len(b):
  sec=b[p];p+=1;sz,p=leb(b,p);end=p+sz
  if sec==7:
   n,p=leb(b,p)
   for _ in range(n):
    sz,p=leb(b,p);name=b[p:p+sz];p+=sz;kind=b[p];p+=1;idx,p=leb(b,p)
    if kind==0 and name==b"_start":return idx
  p=end
 raise ValueError("no guest entry")
class Console:
 def __init__(self,case,policy):
  self.name=case["name"]+"-"+policy;self.log=bytearray();self.pending=bytearray();self.commands=[]
  self.argv=[*runner,str(binary),"-Rdbg","-Rct","0","-Rllvm-cache-path","disable","-Rllvm-call-stack",policy,*["-WFE-"+f for f in case["features"]],"--run",str(TD/(case["name"]+".wasm"))]
  self.child=subprocess.Popen(self.argv,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,preexec_fn=lambda:time.sleep(.15))
  self.sel=selectors.DefaultSelector();self.sel.register(self.child.stdout,selectors.EVENT_READ)
 def prompt(self):
  deadline=time.monotonic()+a.prompt_timeout;mark=b"(uwvm-debug) "
  while mark not in self.pending:
   ready=self.sel.select(max(0,deadline-time.monotonic()));assert ready,(self.name,"prompt timeout",self.log[-6000:])
   b=os.read(self.child.stdout.fileno(),65536);assert b,(self.name,"guest exited before prompt",self.child.poll(),self.log[-6000:])
   self.log.extend(b);self.pending.extend(b);assert len(self.log)<2**23
  p=self.pending.index(mark)+len(mark);r=bytes(self.pending[:p]);del self.pending[:p];return r
 def send(self,cmd):
  self.commands.append(cmd);self.child.stdin.write(cmd.encode()+b"\n");self.child.stdin.flush();return self.prompt()
 def stopped(self):
  limit=time.monotonic()+a.stop_timeout
  while True:
   r=self.send("status")
   if b"stopped:" in r:return r
   assert b"guest exited" not in r,r
   assert time.monotonic()<limit,r
   time.sleep(.02)
 def saveclose(self,success):
  closed=False
  try:
   if self.child.poll() is None:
    self.child.stdin.write(b"quit\n");self.child.stdin.flush()
    self.child.stdin.close()
    try:self.child.wait(timeout=10)
    except subprocess.TimeoutExpired:
     self.child.kill();self.child.wait(timeout=5)
     raise AssertionError("managed debug quit did not retire within 10 seconds")
   self.log.extend(self.child.stdout.read())
   assert self.child.returncode == 0,("managed debug quit",self.child.returncode,self.log[-4000:])
   closed=True
  finally:
   if self.child.poll() is None:self.child.kill();self.child.wait(timeout=5)
   if self.child.poll() is not None:self.log.extend(self.child.stdout.read())
   self.sel.close()
   (out/(self.name+".log")).write_bytes(self.log)
   (out/(self.name+".commands.json")).write_text(json.dumps({"argv":self.argv,"commands":self.commands,"passed":success and closed,"exit_code":self.child.returncode},indent=2)+"\n")
for case in cases:
 for policy in ["instruction","unwind"]:
  c=None;passed=False;record={"feature":case["name"],"policy":policy,"actual_VM":True,"execution_passed":False,"debug_queries_passed":False,"query_scope":"entry stop smoke; not exhaustive feature state coverage","queries":[]}
  try:
   c=Console(case,policy);c.prompt();entry=exported(TD/(case["name"]+".wasm"))
   assert b"prepared; no Wasm instruction executed" in c.send("status")
   assert b"registered" in c.send("break 0 "+str(entry)+" 0")
   assert b"error:" not in c.send("trace wasm on")
   c.send("continue");r=c.stopped()
   who=re.search(rb"thread ([0-9]+) module=0 function="+str(entry).encode()+rb" ",r);assert who,r
   thread=int(who[1]);stop_match=re.search(rb"^stop-id ([0-9]+)$",r,re.M);assert stop_match,r
   stop=int(stop_match[1])
   for cmd in ["bt "+str(thread),"locals wasm "+str(thread),"operands "+str(thread),"globals "+str(thread)+" 0","controls "+str(thread),"handlers "+str(thread),"saved "+str(thread)]:
    answer=c.send(cmd);record["queries"].append(query_evidence(cmd,answer,participant=thread,stop=stop))
   assert b"breakpoint deleted" in c.send("delete 1")
   for _ in range(3):
    r=c.send("step wasm "+str(thread));assert b"selected participant step" in r or b"guest exited: 0" in r,r
    if b"guest exited: 0" in r:break
   else:
    c.send("continue")
   limit=time.monotonic()+a.stop_timeout
   while True:
    r=c.send("status")
    if b"guest exited:" in r:
     assert b"guest exited: 0" in r,r;break
    assert time.monotonic()<limit,r;time.sleep(.02)
   trace=c.send("trace wasm read 0 128");assert b"error:" not in trace,trace
   assert b"wasm-event sequence=" in trace and b"opcode=unavailable" not in trace,trace
   required={"simd":b"fd:","relaxed-simd":b"fd:","gc":b"fb:","threads":b"fe:","bulk-memory":b"fc:"}
   for feature,opcode in required.items():
    if feature in case["features"] and (feature != "bulk-memory" or case["name"] in ("bulk-memory","tables-references","multi-memory")):assert opcode in trace.lower(),(feature,trace)
   record["execution_passed"]=True
   record["debug_queries_passed"]=all(q["available"] for q in record["queries"])
   passed=record["debug_queries_passed"]
   if not passed:record["error"]="one or more current entry-stop state queries unavailable or invalid"
  except BaseException as e:
   record["error"]=repr(e);record["traceback"]=traceback.format_exc()
  finally:
   if c:
    try:
     c.saveclose(passed);record["managed_quit_passed"]=True
    except Exception as e:
     passed=False;record["managed_quit_passed"]=False;record["close_error"]=repr(e)
    record["exit_code"]=c.child.returncode
  record["passed"]=passed;rows.append(record);(out/"results.json").write_text(json.dumps(rows,indent=2)+"\n");print(json.dumps(record),flush=True)
raise SystemExit(0 if rows and all(r["passed"] for r in rows) else 1)

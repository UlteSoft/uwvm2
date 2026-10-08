#!/usr/bin/env python3
"""Standard integer-specifier permutations against actual C++ type/value output; finite copied DATA."""
from pathlib import Path
import hashlib,importlib.util,json,random,subprocess,sys,time
sys.dont_write_bytecode=True
def verify(root,command,out,old_adapter=None):
 root,out=Path(root),Path(out);assert sys.platform=="linux"
 subprocess.run(["bash",str(root/"tools/ci/require_wasm3_test_cgroup.sh")],check=True)
 out.mkdir(exist_ok=False);pins={}
 def pin(p):p=Path(p);pins[str(p)]=hashlib.file_digest(p.open("rb"),"sha256").hexdigest()
 def module(path,name):
  spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
 pin(__file__);pin(root/"tools/debug/dap_adapter.py");dap=module(root/"tools/debug/dap_adapter.py","spaced_dap")
 old=module(old_adapter,"old_dap") if old_adapter else None
 if old_adapter:pin(old_adapter)
 rng=random.Random(20261020);cases=[]
 # Independent canonical declarations from the standard integer-type table.
 from itertools import permutations
 declarations=[(s+" "+b if s else b,w,s=="unsigned") for s in ["","signed","unsigned"]
  for b,w in [("char",8),("int",32),("short",16),("short int",16),("long",0),("long int",0),("long long",64),("long long int",64)]]
 declarations += [("signed",32,False),("unsigned",32,True)]
 types={}
 for canonical,width,uns in declarations:
  for order in set(permutations(canonical.split())):types[" ".join(order)]=(width,uns)
 def add(name,width,uns,mode,spaced):
  spelling=("   " if spaced else " ").join(name.split());g="  " if spaced else ""
  if mode==0:expr="static_cast"+g+"<"+g+spelling+g+">"+g+"("+g+"-73"+g+")";bits=-73;reads=0
  elif mode==1:expr="("+g+spelling+g+")"+g+"("+g+"value"+g+")";bits=42;reads=1
  elif mode==2:expr="sizeof"+g+"("+g+spelling+g+")";bits=None;reads=0
  else:expr="static_cast"+g+"<"+g+spelling+g+">"+g+"("+g+"flag"+g+")";bits=1;reads=1
  expected={}
  for guest in [32,64]:
   w=width or guest
   expected[str(guest)]={"bits":w//8 if mode==2 else bits&((1<<w)-1),"width":guest if mode==2 else w,
    "unsigned":True if mode==2 else uns,"floating":False,"reads":reads,"queries":0,"root":1 if mode==1 else 2 if mode==3 else 0}
  cases.append({"expression":expr,"accepted":True,"expected":expected})
 for name,(width,uns) in sorted(types.items()):
  for mode in range(4):
   for spaced in [False,True]:add(name,width,uns,mode,spaced)
 for i in range(1024):
  name,(width,uns)=rng.choice(sorted(types.items()));add(name,width,uns,i%4,True)
 for text,bits,code in [("signed + 1",14,8),("signed_value + 1",15,9),("static_cast + 1",10,4),("static_cast_value < value",1,1)]:
  cases.append({"expression":text,"accepted":True,"expected":{str(g):{"bits":bits,"width":32,"unsigned":False,"floating":False,"reads":2 if code==1 else 1,"queries":0,"root":code} for g in [32,64]}})
 bad=["signed unsigned","signed signed","unsigned unsigned","char int","int char","char char","short short","int int","long long long",
  "short long","long short","long char","char unsigned int","signed unsigned long","unsigned short int int","long long int int",
  "unsigned i32","signed u64","long bool","short float","unsigned double","long double","long long double",
  "unsignedchar","unsigned shortint","un signed int","int*","unsigned long&","unsigned long [1]","unsigned long ()",
  "unsigned int; continue","unsigned int\ncontinue"]
 invalid=[]
 for name in bad:
  invalid.extend(["static_cast < "+name+" > (value)","("+name+")(value)","0 && static_cast<"+name+">(value)"])
 invalid += ["@as (signed, 1)","@as (unsigned short int, 1)","static_cast<signed>(value = 8)","static_cast<unsigned long long int>(value++)",
  "static_cast<signed>(value).field","sizeof(signed signed)","sizeof(unsigned short long)"]
 cases += [{"expression":text,"accepted":False} for text in invalid]
 record={"passed":False,"cases":len(cases),"rows":[],"inputs":pins,"old_DAP_refusals":0,"scope":__doc__,"live_native_producer_parity":False,"cgroup":Path("/proc/self/cgroup").read_text()}
 assert all(len(v["expression"])<=256 for v in cases)
 cp=out/"cases.json";cp.write_text(json.dumps(cases,separators=(",",":"))+"\n");pin(cp)
 for row in cases:
  try:dap.validate_source_evaluation_expression(row["expression"]);accepted=True
  except ValueError:accepted=False
  assert accepted==row["accepted"],("DAP acceptance",row,accepted)
  if old and row["accepted"]:
   try:old.validate_source_evaluation_expression(row["expression"])
   except ValueError:record["old_DAP_refusals"]+=1
 if old:assert record["old_DAP_refusals"]>0
 for start in range(0,len(cases),128):
  batch=cases[start:start+128];log=out/("batch-"+str(start)+".log");began=time.monotonic()
  with log.open("wb") as f:r=subprocess.run([*command,*[v["expression"] for v in batch]],stdout=f,stderr=subprocess.STDOUT,timeout=90)
  assert r.returncode==0,(r.returncode,log.read_text()[-2000:])
  lines=log.read_text().splitlines();assert len(lines)==len(batch)*2
  for i,row in enumerate(batch):
   for j,guest in enumerate([32,64]):
    v=list(map(int,lines[2*i+j].split("\t")));assert len(v)==11 and v[:3]==[i,guest,int(row["accepted"])],("parse",row,v)
    if row["accepted"]:
     e=row["expected"][str(guest)]
     assert v[3:]==[0,e["bits"],e["width"],int(e["unsigned"]),int(e["floating"]),e["reads"],e["queries"],e["root"]],("native type/value/callback oracle",row,v)
    else:assert v[8:]==[0,0,0],("rejected syntax callbacks",row,v)
  pin(log);record["rows"].append({"start":start,"cases":len(batch),"actual_CXX_rows":len(lines),"seconds":time.monotonic()-began,"log":str(log),"sha256":pins[str(log)]})
 assert all(hashlib.file_digest(Path(p).open("rb"),"sha256").hexdigest()==h for p,h in pins.items())
 record.update(passed=True,inputs_after_unchanged=True)
 (out/"results.json").write_text(json.dumps(record,indent=2)+"\n");return record

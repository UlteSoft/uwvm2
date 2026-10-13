#!/usr/bin/env python3
"""Token spacing against actual C++ conversion output; finite copied DATA."""
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
 rng=random.Random(20261019);cases=[]
 types=[("int",32,False,False,"-11",-11),("unsigned char",8,True,False,"255",255),
 ("signed char",8,False,False,"-123",-123),("long long int",64,False,False,"-19",-19),
 ("unsigned short",16,True,False,"257",257),("float",32,False,True,"0.5",0x3f000000),
 ("double",64,False,True,"1.25",0x3ff4000000000000),("long",0,False,False,"-3",-3)]
 def gap():return " "*rng.randrange(5)
 for i in range(1024):
  name,width,uns,real,literal,bits=types[i%len(types)]
  spelling=(" "*rng.randrange(1,5)).join(name.split())
  mode=i%5
  if mode==0:expr=gap()+"static_cast"+gap()+"<"+gap()+spelling+gap()+">"+gap()+"("+gap()+literal+gap()+")"+gap()
  elif mode==1:expr="("+gap()+spelling+gap()+")"+gap()+"("+literal+")"
  elif mode==2:
   expr="sizeof"+gap()+"("+gap()+spelling+gap()+")";bits=None;uns=True;real=False
  elif mode==3:
   name,width,uns,real,literal,bits=("u8",8,True,False,"255",255) if i&1 else ("i32",32,False,False,"42",42)
   expr="@as"+gap()+"("+gap()+name+gap()+","+gap()+literal+gap()+")"
  else:
   expr="static_cast"+gap()+"<"+gap()+spelling+gap()+">"+gap()+"("+gap()+"flag"+gap()+")"
   bits=(0x3f800000 if width==32 else 0x3ff0000000000000) if real else 1
  rows={}
  for guest in [32,64]:
   w=width or guest
   rows[str(guest)]={"bits":w//8 if mode==2 else bits if real else bits&((1<<w)-1),
    "width":guest if mode==2 else w,"unsigned":bool(uns),"floating":bool(real),"reads":int(mode==4),"queries":0,"root":2 if mode==4 else 0}
  cases.append({"expression":expr,"accepted":True,"expected":rows})
 for text,bits,reads,code in [("static_cast_value < value",1,2,1),("static_cast + 1",10,1,4),("aspect + 1",11,1,5),("as_value + 1",12,1,6),("as + 1",13,1,7)]:
  cases.append({"expression":text,"accepted":True,"expected":{str(g):{"bits":bits,"width":32,"unsigned":False,"floating":False,"reads":reads,"queries":0,"root":code} for g in [32,64]}})
 invalid=["static_castx<int>(value, 1)","sta tic_cast<int>(value)","@asx (i32, value)","static_cast < int* > (value)",
 "@as ( *i32, value)","static_cast < unsignedchar > (value)","(unsignedchar)(value)","@as (u 8, 1)",
 "static_cast < int > (value = 8)","@as (u8, value++)","0 && static_cast < int* > (value)","1 || @as (u 8, 1)",
 "static_cast < int > (value, 1)","@as (u8, 1, 2)","static_cast < int > value","@as (u8)",
 "static_cast < int > (value) .field","sizeof(unsignedchar) + 1"]
 # Unknown sizeof identifier is syntactically a producer reference; exclude from parse refusal.
 invalid=invalid[:-1]
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

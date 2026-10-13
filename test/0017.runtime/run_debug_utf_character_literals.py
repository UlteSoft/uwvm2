"""Bounded UTF syntax/DAP properties against an actual production C++ probe."""
from pathlib import Path
import hashlib, importlib.util, json, random, subprocess, time
def sha(p):
 with Path(p).open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()
def verify(root, argv_prefix, output, count=10000):
 root,output=Path(root),Path(output)
 subprocess.run(["bash",str(root/"tools/ci/require_wasm3_test_cgroup.sh")],check=True)
 module=root/"tools/debug/dap_adapter.py";pins={str(p):sha(p) for p in (Path(__file__),module)}
 spec=importlib.util.spec_from_file_location("utf_property_dap",module);dap=importlib.util.module_from_spec(spec);spec.loader.exec_module(dap)
 rng=random.Random(20261008);rows=[]
 for _ in range(count):
  width=rng.choice((8,16,32));value=rng.randrange(1<<width)
  prefix={8:"u8",16:"u",32:"U"}[width];literal=prefix+"'\\x"+format(value,"x")+"'"
  choice=rng.randrange(6);uns=width==32
  if choice==0:expr,bits,size,unsigned,kind=literal,value,width,True,{8:"char8_t",16:"char16_t",32:"char32_t"}[width]
  elif choice==1:expr,bits,size,unsigned,kind="+"+literal,value,32,uns,"unsigned int" if uns else "int"
  elif choice==2:expr,bits,size,unsigned,kind="sizeof("+literal+")",width//8,32,True,"unsigned long"
  elif choice==3:
   n=rng.randrange(100);expr,bits,size,unsigned,kind=literal+" + "+str(n),(value+n)&0xffffffff,32,uns,"unsigned int" if uns else "int"
  elif choice==4:
   n=rng.randrange(32);expr,bits,size,unsigned,kind=literal+" >> "+str(n),value>>n,32,uns,"unsigned int" if uns else "int"
  else:expr,bits,size,unsigned,kind="1 ? "+literal+" : "+prefix+"'a'",value,width,True,{8:"char8_t",16:"char16_t",32:"char32_t"}[width]
  rows.append(dict(expression=expr,bits=bits,width=size,unsigned=int(unsigned),kind=kind))
 assert len({r["expression"] for r in rows})>count*3//4
 for row in rows:assert dap.validate_source_evaluation_expression(row["expression"])==row["expression"],row
 args=[r["expression"] for r in rows];assert sum(len(s.encode())+1 for s in args)<700000
 start=time.monotonic()
 with output.open("wb") as f:r=subprocess.run([*argv_prefix,*args],stdout=f,stderr=subprocess.STDOUT,timeout=120)
 assert r.returncode==0,("actual UTF probe exit",r.returncode)
 lines=output.read_text().splitlines();assert len(lines)==len(rows)
 for i,(line,expected) in enumerate(zip(lines,rows)):
  fields=line.split("\t");want=[str(i),"0","0",str(expected["bits"]),str(expected["width"]),str(expected["unsigned"]),expected["kind"],"0","0"]
  assert fields==want,(expected,fields,want)
 assert all(sha(p)==h for p,h in pins.items())
 return dict(passed=True,checks=len(rows),seed=20261008,inputs=pins,log_sha256=sha(output),seconds=time.monotonic()-start,actual_VM_qualified=False)

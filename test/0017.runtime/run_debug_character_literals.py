"""Finite character syntax/numeric DATA verifier; run only under the original cgroup supervisor."""
from pathlib import Path
import hashlib,importlib.util,json,random,subprocess,sys,time
sys.dont_write_bytecode=True
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def cases(root,count=10000):
 corpus=json.loads((root/'test/0018.debugger/dap_character_literal_cases.json').read_text());rows=corpus['cases'][:]
 literals=[(v['expression'],v['bits']) for v in rows if v['accepted'] and v['expression'].startswith("'") and v['expression'].endswith("'") and v['reads']==0]
 assert len(literals)>=99
 rng=random.Random(20261006);generated=[]
 for _ in range(count):
  left,x=rng.choice(literals);right,y=rng.choice(literals);op=rng.choice(('+','-','*','/','%','&','|','^','&^','<<','>>','==','!=','<','<=','>','>='))
  if op in ('/','%') and y==0:right,y='1',1
  if op in ('<<','>>'):y=rng.randrange(8);right=str(y)
  value={'+':lambda:x+y,'-':lambda:x-y,'*':lambda:x*y,'/':lambda:x//y,'%':lambda:x%y,'&':lambda:x&y,'|':lambda:x|y,'^':lambda:x^y,'&^':lambda:x&~y,'<<':lambda:x<<y,'>>':lambda:x>>y,'==':lambda:int(x==y),'!=':lambda:int(x!=y),'<':lambda:int(x<y),'<=':lambda:int(x<=y),'>':lambda:int(x>y),'>=':lambda:int(x>=y)}[op]()
  generated.append({'expression':'('+left+' '+op+' '+right+')','accepted':True,'bits':value&0xffffffff,'reads':0})
 assert len({v['expression'] for v in generated})>count*3//4
 return rows+generated,{'golden_positive':corpus['positive'],'golden_negative':corpus['negative'],'arithmetic_properties':count,'seed':20261006}
def verify(root,argv_prefix,out,timeout=120):
 root,out=Path(root),Path(out)
 subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 module=root/'tools/debug/dap_adapter.py';pins={str(p):sha(p) for p in (Path(__file__),module,root/'test/0018.debugger/dap_character_literal_cases.json')}
 spec=importlib.util.spec_from_file_location('character_dap',module);dap=importlib.util.module_from_spec(spec);spec.loader.exec_module(dap)
 rows,counts=cases(root)
 for row in rows:
  try:normalized=dap.validate_source_evaluation_expression(row['expression'])
  except ValueError:assert not row['accepted'],('DAP refusal',row)
  else:assert row['accepted'] and normalized==row['expression'],('DAP syntax mismatch',row,normalized)
 args=[v['expression'] for v in rows];assert sum(len(v.encode())+1 for v in args)<1400000
 started=time.monotonic()
 with out.open('wb') as f:r=subprocess.run([*argv_prefix,'--probe',*args],stdout=f,stderr=subprocess.STDOUT,timeout=timeout)
 assert r.returncode==0,('C++ probe failed',r.returncode)
 lines=out.read_text().splitlines();assert len(lines)==len(rows),(len(lines),len(rows))
 for i,(line,expected) in enumerate(zip(lines,rows)):
  fields=list(map(int,line.split('\t')));assert len(fields)==8 and fields[0]==i,(i,line)
  if expected['accepted']:assert fields==[i,0,0,expected['bits'],32,0,0,expected['reads']],(expected,fields)
  else:assert fields[1]!=0 and fields[2]!=0 and fields[3]==0 and fields[7]==0,(expected,fields)
 assert all(sha(p)==h for p,h in pins.items())
 return {'passed':True,**counts,'checks':len(rows),'inputs':pins,'argv_prefix':argv_prefix,'log_sha256':sha(out),'seconds':time.monotonic()-started,'live_VM_qualified':False,'complete_native_semantics_qualified':False}

#!/usr/bin/env python3
"""Finite common scalar arithmetic properties against actual frozen C++ ELFs.

Requires a completed guarded x86_64 debug_source_dap_expression build in both
repositories. This is owned DATA, not a real language pause or a Go/Rust/Zig
semantic engine. Generated expressions are fully parenthesized shared integer
syntax, with independently computed bounded signed 32-bit arithmetic.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,random,subprocess,sys,time
sys.dont_write_bytecode=True
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--root',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
ap.add_argument('--build-record',type=Path,required=True);ap.add_argument('--count',type=int,default=10000)
a=ap.parse_args();assert 1000<=a.count<=20000;a.out.mkdir(exist_ok=False)
subprocess.run(['bash',str(a.root/'uwvm2/tools/ci/require_wasm3_test_cgroup.sh')],check=True)
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
pins={};record={'passed':False,'scope':__doc__,'seed':20261005,'count_per_repository':a.count,
 'rows':[],'cgroup':Path('/proc/self/cgroup').read_text(),'inputs':pins,
 'full_language_native_semantics_qualified':False,'live_VM_qualified':False}
def pin(p,h=None):
 p=Path(p);actual=sha(p);assert h is None or actual==h,('input changed',str(p));pins[str(p)]=actual
def publish():(a.out/'results.json').write_text(json.dumps(record,indent=2)+'\n')
pin(Path(__file__));pin(a.build_record);pin(Path(sys.executable).resolve())
base=json.loads(a.build_record.read_text());assert base['passed'] and base['inputs_after_unchanged']
for p,h in base['inputs'].items():pin(p,h)
rng=random.Random(20261005);low=-(1<<31);high=(1<<31)-1
def quotient(x,y):
 q=abs(x)//abs(y);return -q if (x<0)!=(y<0) else q
def expression(depth):
 if depth==0 or rng.random()<.22:
  value=rng.randrange(-1000,1001);return str(value),value
 if rng.random()<.2:
  text,value=expression(depth-1);op=rng.choice(['+','-','~','!'])
  actual=value if op=='+' else -value if op=='-' else ~value if op=='~' else int(not value)
  if not low<=actual<=high:return text,value
  return '('+op+' ('+text+'))',actual
 left,x=expression(depth-1);right,y=expression(depth-1)
 op=rng.choice(['+','-','*','/','%','&','&^','|','^','<<','>>','==','!=','<','<=','>','>=','&&','||'])
 if op in ('<<','>>'):
  y=rng.randrange(13);right=str(y)
  if op=='<<' and x<0:return left,x
 if op in ('/','%') and not y:return left,x
 if op=='+':value=x+y
 elif op=='-':value=x-y
 elif op=='*':value=x*y
 elif op=='/':value=quotient(x,y)
 elif op=='%':value=x-quotient(x,y)*y
 elif op=='&':value=x&y
 elif op=='&^':value=x&~y
 elif op=='|':value=x|y
 elif op=='^':value=x^y
 elif op=='<<':value=x<<y
 elif op=='>>':value=x>>y
 elif op=='==':value=int(x==y)
 elif op=='!=':value=int(x!=y)
 elif op=='<':value=int(x<y)
 elif op=='<=':value=int(x<=y)
 elif op=='>':value=int(x>y)
 elif op=='>=':value=int(x>=y)
 elif op=='&&':value=int(bool(x) and bool(y))
 else:value=int(bool(x) or bool(y))
 if not low<=value<=high:return left,x
 return '('+left+' '+op+' '+right+')',value
cases=[]
while len(cases)<a.count:
 text,value=expression(rng.randrange(1,5))
 if len(text)<=220:cases.append({'expression':text,'bits':value&0xffffffff})
assert sum(len(v['expression'])+1 for v in cases)<1400000,'bounded process argument size'
corpus=a.out/'properties.json';corpus.write_text(json.dumps(cases,separators=(',',':'))+'\n');pin(corpus)
record['unique_expressions']=len({v['expression'] for v in cases});assert record['unique_expressions']>a.count//2
for repo in ['uwvm2','uwvm2-ros']:
 rows=[v for v in base['rows'] if v['repository']==repo and v['case']=='debug_source_dap_expression' and v['profile']=='x86_64'];assert len(rows)==1
 row=rows[0];assert row['passed']
 for p,h in row['dependency_hashes'].items():pin(p,h)
 phase=next(v for v in row['phases'] if v['phase']=='qemu-execute');argv=phase['argv']
 binary_index=next(i for i,v in enumerate(argv) if v.endswith('/test.elf'));binary=Path(argv[binary_index]);pin(binary,row['ELF']['sha256']);pin(Path(argv[0]).resolve())
 module=a.root/repo/'tools/debug/dap_adapter.py';pin(module)
 spec=importlib.util.spec_from_file_location('properties_'+repo.replace('-','_'),module);dap=importlib.util.module_from_spec(spec);spec.loader.exec_module(dap)
 for case in cases:assert dap.validate_source_evaluation_expression(case['expression'])==case['expression']
 args=[v['expression'] for v in cases];result={'repository':repo,'passed':False,'syntax_checks':len(cases),'phases':[]}
 for label,command in [('qemu',argv[:binary_index+1]+args),('native',[str(binary)]+args)]:
  log=a.out/(repo+'-'+label+'.log');started=time.monotonic()
  with log.open('wb') as f:r=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,cwd=a.root,timeout=180)
  result['phases'].append({'label':label,'returncode':r.returncode,'seconds':time.monotonic()-started,'log':str(log),'log_sha256':sha(log),
   'argv_prefix':command[:-len(args)],'argument_source':str(corpus),'ELF_sha256':row['ELF']['sha256']})
  assert r.returncode==0,('target failed',repo,label,r.returncode,log.read_text()[-500:])
  lines=log.read_text().splitlines();assert len(lines)==len(cases),('incomplete rows',repo,label)
  for index,(line,expected) in enumerate(zip(lines,cases)):
   fields=list(map(int,line.split('\t')))
   assert fields==[index,0,expected['bits'],32,0,0,0],('scalar property mismatch',repo,label,index,expected,fields)
  pin(log)
 assert result['phases'][0]['log_sha256']==result['phases'][1]['log_sha256']
 result['passed']=True;record['rows'].append(result);publish()
for p,h in pins.items():assert sha(p)==h,p
record['passed']=True;record['inputs_after_unchanged']=True;publish()
print(json.dumps({'passed':True,'repositories':2,'properties_per_repository':a.count,'unique':record['unique_expressions'],'syntax_checks':2*a.count,'actual_CPP_result_checks':4*a.count}))

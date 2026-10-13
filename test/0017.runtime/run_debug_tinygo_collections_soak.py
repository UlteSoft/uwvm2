#!/usr/bin/env python3
"""Repeat the successful original TinyGo builtin collection value sessions.

All VM/Python work must run behind the Linux cgroup birth/PIDFD supervisor.
This finite runtime-backed map/channel subset does not qualify general native Go semantics.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys,time
sys.dont_write_bytecode=True
ap=argparse.ArgumentParser(description=__doc__)
for name in ['live-result','out','source-root']:ap.add_argument('--'+name,type=Path,required=True)
ap.add_argument('--seconds',type=int,default=600);ap.add_argument('--max-sessions',type=int,default=200)
a=ap.parse_args();assert sys.platform=='linux' and 1<=a.seconds<=1800 and 2<=a.max_sessions<=10000
subprocess.run(['bash',str(a.source_root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
O=a.out;O.mkdir(parents=True,exist_ok=False);sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
pins={str(Path(__file__).resolve()):sha(__file__),str(a.live_result.resolve()):sha(a.live_result),str(Path(sys.executable).resolve()):sha(sys.executable)}
base=json.loads(a.live_result.read_text());assert base['passed'] and base['actual_sessions']==2 and len(base['rows'])==2
origins=[]
for row in base['rows']:
 assert row['returncode']==0;rp=Path(row['result']);r=json.loads(rp.read_text());assert r['passed'] and r['guest_completed_without_trap'] and r['quit_returncode']==0 and r['managed_shutdown_complete'] and len(r['values'])==30 and all(v['passed'] for v in r['values'])
 pins[str(rp)]=sha(rp)
 for p,h in r['inputs'].items():assert sha(p)==h;pins[p]=h
 argv=list(row['argv']);assert argv[0]==sys.executable and argv[argv.index('--out')+1]==str(rp.parent);origins.append((argv,r['policy']))
assert {v[1] for v in origins}=={'instruction','unwind'}
record={'passed':False,'scope':__doc__,'maximum_seconds':a.seconds,'maximum_sessions':a.max_sessions,'inputs':pins,'sessions':[],'value_comparisons':0,'cgroup':Path('/proc/self/cgroup').read_text()}
started=time.monotonic()
def save():
 record['elapsed_seconds']=time.monotonic()-started;(O/'results.json').write_text(json.dumps(record,indent=2)+'\n')
save()
try:
 while time.monotonic()-started<a.seconds and len(record['sessions'])<a.max_sessions:
  index=len(record['sessions']);argv,policy=origins[index%len(origins)];argv=list(argv);case=O/f'{index:05d}';argv[argv.index('--out')+1]=str(case);log=O/f'{index:05d}.log'
  with log.open('wb') as f:p=subprocess.run(argv,stdout=f,stderr=subprocess.STDOUT,timeout=300)
  row={'index':index,'policy':policy,'passed':False,'returncode':p.returncode,'log_sha256':sha(log),'result':str(case/'results.json')};record['sessions'].append(row);save()
  assert p.returncode==0,log.read_text()[-4000:];r=json.loads((case/'results.json').read_text());assert r['passed'] and r['quit_returncode']==0 and r['managed_shutdown_complete'] and r['guest_completed_without_trap'] and len(r['values'])==30 and all(v['passed'] for v in r['values'])
  assert r['policy']==policy and r['compiler_oracles']==dict(mapLen=3,namedLen=2,bufferedLen=2,bufferedCap=4,closedLen=1,closedCap=3)
  row.update(passed=True,quit_returncode=0,managed_shutdown_complete=True,value_comparisons=30,result_sha256=sha(case/'results.json'));record['value_comparisons']+=30;save()
  if (index+1)%10==0:print('TinyGo builtin SOAK',index+1,record['value_comparisons'],round(record['elapsed_seconds'],2),flush=True)
 assert len(record['sessions'])>=2 and {r['policy'] for r in record['sessions']}=={'instruction','unwind'} and all(r['passed'] for r in record['sessions'])
 for p,h in pins.items():assert sha(p)==h,p
 record.update(passed=True,inputs_before_equals_after=True);save()
except BaseException as e:record.update(passed=False,error=repr(e));save();raise
print('PASS TinyGo builtin SOAK',len(record['sessions']),record['value_comparisons'],round(record['elapsed_seconds'],2),flush=True)

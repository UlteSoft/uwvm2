#!/usr/bin/env python3
"""Repeat original compiler-backed -Rdbg matrix sessions under a Linux cgroup.

Use the successful full-product matrix JSON from run_debug_language_product_size_cli.
All inputs are pinned; the caller supplies a separate exclusive output directory.
Run behind the birth/PIDFD product supervisor, in the designated test cgroup.
"""
from pathlib import Path
import argparse,hashlib,json,re,sys,time
sys.dont_write_bytecode=True
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--source-root',type=Path,required=True)
ap.add_argument('--matrix-result',type=Path,required=True)
ap.add_argument('--out',type=Path,required=True)
ap.add_argument('--seconds',type=int,default=600)
ap.add_argument('--max-sessions',type=int,default=2000)
a=ap.parse_args();assert sys.platform=='linux'
S=a.source_root.resolve(strict=True);O=a.out.resolve();O.mkdir(parents=True,exist_ok=False)
sys.path.insert(0,str(S/'test/0017.runtime'));from run_debug_source_inline_metadata_cli import Console
import subprocess
subprocess.run(['bash',str(S/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
assert 1<=a.seconds<=3600 and 1<=a.max_sessions<=100000
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
matrix=a.matrix_result.resolve(strict=True);M=json.loads(matrix.read_text());repository=M['repository'];assert M['passed'] and len(M['cases'])==40 and all(r['passed'] and len(r['checks'])==18 and all(c['passed'] for c in r['checks']) and r['quit_returncode']==0 and r['managed_shutdown_complete'] for r in M['cases']),'Incomplete successful matrix required'
fixture=S/'test/0017.runtime/fixtures/debug_language_product_size.c';line=next(i for i,s in enumerate(fixture.read_text().splitlines(),1) if 'LANGUAGE_PRODUCT_READY' in s)
pins={str(Path(__file__).resolve()):sha(__file__),str(matrix):sha(matrix),str(fixture):sha(fixture),str(S/'test/0017.runtime/run_debug_source_inline_metadata_cli.py'):sha(S/'test/0017.runtime/run_debug_source_inline_metadata_cli.py')}
for row in M['cases']:
 assert sha(row['argv'][0])==M['product_sha256'] and sha(row['argv'][-1])==row['wasm_sha256']
 for p in [row['argv'][0],row['argv'][-1]]:pins[p]=sha(p)
record={'passed':False,'repository':repository,'matrix_source_id':M['build_source_id'],'product_sha256':M['product_sha256'],'maximum_seconds':a.seconds,'maximum_sessions':a.max_sessions,'inputs':pins,'sessions':[],'checks':0,'cgroup':Path('/proc/self/cgroup').read_text(),'scope':'Actual full-product repeated source stops, target-compiled oracle reads, retired/fabricated stop refusal, and complete guest/managed shutdown'}
def save():(O/'results.json').write_text(json.dumps(record,indent=2)+'\n')
def number(text):
 assert 'error:' not in text and 'unavailable' not in text and 'rejected' not in text,text
 v=re.findall(r'(?m)^(?!source-value\b)[^\r\n]*\b(?:value=|[ui](?:8|16|32|64)=)(true|false|-?\d+)\b',text);assert len(v)==1,text
 return 1 if v[0]=='true' else 0 if v[0]=='false' else int(v[0])
save();started=time.monotonic()
while time.monotonic()-started<a.seconds and len(record['sessions'])<a.max_sessions:
 index=len(record['sessions']);origin=M['cases'][index%len(M['cases'])];log=O/(f'{index:05d}.log');row={'index':index,'stem':origin['stem'],'policy':origin['policy'],'passed':False,'checks':0};record['sessions'].append(row);console=None
 try:
  console=Console(origin['argv'],log)
  def ask(command):return re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',console.send(command)).decode(errors='replace')
  br=ask(f'break-source 0 {fixture}:{line}');bid=int(re.search(r'breakpoint (\d+)',br)[1]);ask('continue')
  for _ in range(64):
   stop=ask('wait')
   if 'stopped: breakpoint' in stop:break
   assert 'guest exited:' not in stop,stop
  else:raise AssertionError('No actual source stop')
  thread=int(re.search(r'thread (\d+) module=0 function=',stop)[1]);sid=int(re.search(r'(?m)^stop-id (\d+)',ask(f'bt {thread}'))[1])
  for i,check in enumerate(origin['checks']):
   oracle=number(ask(f'print {thread} {sid} oracle{i}'));value=number(ask(f'print {thread} {sid} '+check['expression']))
   assert value==oracle==check['target_compiler_oracle'],(check['expression'],value,oracle)
   row['checks']+=1;record['checks']+=1
  stale=ask(f'print {thread} {sid+1000000} value');assert 'error:' in stale and 'source-value stop=' not in stale
  ask(f'delete {bid}');assert 'guest exited:' not in ask(f'step wasm {thread}')
  stale=ask(f'print {thread} {sid} value');assert 'error:' in stale and 'source-value stop=' not in stale
  ask('continue')
  for _ in range(64):
   ended=ask('wait')
   if 'guest exited:' in ended:assert 'guest exited: 0' in ended;break
  else:raise AssertionError('Guest exit deadline')
  row['passed']=True
 except BaseException as error:row['error']=repr(error);save();raise
 finally:
  if console:
   try:
    console.finish();row['quit_returncode']=console.child.returncode;row['managed_shutdown_complete']='managed shutdown complete' in bytes(console.transcript).decode(errors='replace');assert row['quit_returncode']==0 and row['managed_shutdown_complete']
   except BaseException as error:row.update(passed=False,shutdown_error=repr(error));save();raise
   finally:
    if log.exists():row.update(log_sha256=sha(log),log_bytes=log.stat().st_size)
    record['elapsed_seconds']=time.monotonic()-started;save()
 if (index+1)%100==0:print(repository,'SOAK',index+1,'sessions',record['checks'],'actual comparisons',round(record['elapsed_seconds'],2),'seconds',flush=True)
for p,h in pins.items():assert sha(p)==h,('Input changed',p)
assert record['sessions'] and all(r['passed'] and r['checks']==18 and r['quit_returncode']==0 and r['managed_shutdown_complete'] for r in record['sessions']),'No complete successful session'
record.update(passed=True,inputs_before_equals_after=True,all_sessions_passed=all(r['passed'] for r in record['sessions']),elapsed_seconds=time.monotonic()-started);save();print(repository,'PASS SOAK',len(record['sessions']),record['checks'],round(record['elapsed_seconds'],2),flush=True)

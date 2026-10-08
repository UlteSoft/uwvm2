#!/usr/bin/env python3
"""Verify actual replacement validation and stale source/generation boundaries.

Use the successful full-product matrix JSON from run_debug_language_product_size_cli.
All inputs are pinned; the caller supplies a separate exclusive output directory.
Run behind the birth/PIDFD product supervisor, in the designated test cgroup.
"""
from pathlib import Path
import argparse,hashlib,json,re,sys
sys.dont_write_bytecode=True
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--source-root',type=Path,required=True)
ap.add_argument('--matrix-result',type=Path,required=True)
ap.add_argument('--out',type=Path,required=True)
a=ap.parse_args();assert sys.platform=='linux'
S=a.source_root.resolve(strict=True);O=a.out.resolve();O.mkdir(parents=True,exist_ok=False)
sys.path.insert(0,str(S/'test/0017.runtime'));from run_debug_source_inline_metadata_cli import Console,function
import subprocess
subprocess.run(['bash',str(S/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
matrix=a.matrix_result.resolve(strict=True);M=json.loads(matrix.read_text());repository=M['repository'];assert M['passed'] and len(M['cases'])==40 and all(r['passed'] and len(r['checks'])==18 and all(c['passed'] for c in r['checks']) and r['quit_returncode']==0 and r['managed_shutdown_complete'] for r in M['cases']),'Incomplete successful matrix required'
selected=[r for r in M['cases'] if r['stem'] in ['c-wasm32-dwarf5','cpp-wasm64-dwarf5']]
assert len(selected)==4 and {(r['stem'],r['policy']) for r in selected}=={(s,p) for s in ['c-wasm32-dwarf5','cpp-wasm64-dwarf5'] for p in ['instruction','unwind']},'Missing or duplicate replacement configuration'
pins={str(Path(__file__).resolve()):sha(__file__),str(matrix):sha(matrix),str(S/'test/0017.runtime/run_debug_source_inline_metadata_cli.py'):sha(S/'test/0017.runtime/run_debug_source_inline_metadata_cli.py')}
record={'passed':False,'repository':repository,'product_sha256':M['product_sha256'],'source_id':M['build_source_id'],'inputs':pins,'cases':[],'scope':'Actual current full debugger negative Wasm-body validation, exact valid replacement, expected-generation refusal, and old source metadata refusal'}
def save():(O/'results.json').write_text(json.dumps(record,indent=2)+'\n')
for origin in selected:
 wasm=Path(origin['argv'][-1]);binary=Path(origin['argv'][0]);assert sha(wasm)==origin['wasm_sha256'] and sha(binary)==M['product_sha256'];pins[str(wasm)]=sha(wasm);pins[str(binary)]=sha(binary)
 target,body=function(wasm,'language_product_size');stem=origin['stem']+'-'+origin['policy'];valid=O/(stem+'-valid.bin');bad=O/(stem+'-bad.bin');valid.write_bytes(body);bad.write_bytes(bytes([0,0x41,1,0x0b]));pins[str(valid)]=sha(valid);pins[str(bad)]=sha(bad)
 row={'stem':origin['stem'],'policy':origin['policy'],'target_function':target,'body_sha256':sha(valid),'bad_body_sha256':sha(bad),'passed':False,'actions':[]};record['cases'].append(row);save();console=None
 try:
  console=Console(origin['argv'],O/(stem+'.log'))
  def ask(command):
   text=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',console.send(command)).decode(errors='replace');row['actions'].append({'command':command,'reply':text});save();return text
  status=ask('status');assert 'prepared; no Wasm instruction executed' in status,status
  invalid=ask(f'replace 0 {target} 1 {bad}');assert 'error:' in invalid or 'rejected' in invalid,invalid
  replaced=ask(f'replace 0 {target} 1 {valid}');assert 'function replaced; generation 2' in replaced,replaced
  stale=ask(f'replace 0 {target} 1 {valid}');assert 'error:' in stale or 'rejected' in stale,stale
  br=ask(f'break 0 {target} 0');bid=int(re.search(r'breakpoint (\d+)',br)[1]);ask('continue')
  for _ in range(64):
   stop=ask('wait')
   if 'stopped: breakpoint' in stop:break
   assert 'guest exited:' not in stop,stop
  else:raise AssertionError('Actual replaced function entry did not stop')
  thread=int(re.search(r'thread (\d+) module=0 function=',stop)[1]);trace=ask(f'bt {thread}');sid=int(re.search(r'(?m)^stop-id (\d+)',trace)[1])
  for command in [f'print {thread} {sid} value',f'ptype {thread} {sid} value']:
   reply=ask(command);assert ('error:' in reply or 'unavailable' in reply) and 'source-value stop=' not in reply,reply
  locals_=ask(f'locals source {thread}');assert 'captured stop, source or function generation is stale' in locals_ and 'source parameter ' not in locals_ and 'source local ' not in locals_,locals_
  ask(f'delete {bid}');ask('continue')
  for _ in range(64):
   ended=ask('wait')
   if 'guest exited:' in ended:assert 'guest exited: 0' in ended;break
  else:raise AssertionError('Replaced guest did not finish')
  row['passed']=True;save();print(repository,stem,'PASS actual validator/generation/source boundaries',flush=True)
 except BaseException as error:row['error']=repr(error);save();raise
 finally:
  if console:
   try:
    console.finish();row['quit_returncode']=console.child.returncode;row['managed_shutdown_complete']='managed shutdown complete' in bytes(console.transcript).decode(errors='replace')
    assert row['quit_returncode']==0 and row['managed_shutdown_complete'],'Debugger shutdown failed'
   except BaseException as error:row.update(passed=False,shutdown_error=repr(error));raise
   finally:save()
for p,h in pins.items():assert sha(p)==h,('Input changed',p)
assert len(record['cases'])==4 and all(r['passed'] and r['quit_returncode']==0 and r['managed_shutdown_complete'] for r in record['cases'])
record.update(passed=True,inputs_before_equals_after=True);save();print(repository,'PASS HOT REPLACEMENT',len(record['cases']),flush=True)

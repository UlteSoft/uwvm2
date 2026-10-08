#!/usr/bin/env python3
"""Real C producer/VM checks that Go builtin lookahead preserves variable names."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess,sys
sys.dont_write_bytecode=True
from run_debug_source_inline_metadata_cli import Console
ap=argparse.ArgumentParser(description=__doc__)
for name in ['uwvm','build-receipt','wasm','source','out']:ap.add_argument('--'+name,type=Path,required=True)
ap.add_argument('--policy',choices=['instruction','unwind'],required=True);ap.add_argument('--ros',action='store_true');a=ap.parse_args();assert sys.platform=='linux'
root=Path(__file__).resolve().parents[2];subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);a.out.mkdir(exist_ok=False)
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest();build=json.loads(a.build_receipt.read_text());assert build['passed'] and build['all_product_TUs_fresh'] and build['inputs_before_equals_after'] and sha(a.uwvm)==build['binary_sha256']
pins={str(p.resolve()):sha(p) for p in [a.uwvm,a.build_receipt,a.wasm,a.source,Path(__file__),root/'test/0017.runtime/run_debug_source_inline_metadata_cli.py']}
record={'passed':False,'source_id':build['source_id'],'inputs':pins,'policy':a.policy,'values':[],'actions':[]}
def save():(a.out/'results.json').write_text(json.dumps(record,indent=2)+'\n')
console=None;save()
try:
 argv=[str(a.uwvm),'-Rdbg']+([] if a.ros else ['-Rcc','jit','-Rcm','full'])+['-Rct','0','-Rllvm-call-stack',a.policy,'-Rllvm-cache-path','disable','--run',str(a.wasm)];record['argv']=argv;console=Console(argv,a.out/'console.log')
 def ask(command):
  reply=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',console.send(command)).decode(errors='replace');record['actions'].append({'command':command,'reply':reply});save();return reply
 def number(reply):
  assert 'error:' not in reply and 'unavailable' not in reply,reply
  values=re.findall(r'(?m)^(?!source-value\b)[^\r\n]*\b(?:value=|[ui](?:8|16|32|64)=)(-?\d+)\b',reply);assert len(values)==1,reply;return int(values[0])
 line=next(i for i,t in enumerate(a.source.read_text().splitlines(),1) if 'BUILTIN_NAMES_READY' in t);br=ask(f'break-source 0 {a.source}:{line}');bid=int(re.search(r'breakpoint (\d+)',br)[1]);ask('continue')
 for _ in range(64):
  stop=ask('wait')
  if 'stopped: breakpoint' in stop:break
  assert 'guest exited:' not in stop,stop
 else:raise AssertionError('actual source stop missing')
 thread=int(re.search(r'thread (\d+) module=0 function=',stop)[1]);sid=int(re.search(r'(?m)^stop-id (\d+)',ask(f'bt {thread}'))[1]);oracle={k:number(ask(f'print {thread} {sid} {k}')) for k in ['len','cap']};assert oracle==dict(len=7,cap=11)
 for expression,expected in [('len',7),('len ',7),('cap',11),('cap ',11),('len - 1',6),('cap - 1',10),('(len) - 1',6),('(cap)-1',10)]:
  assert number(ask(f'print {thread} {sid} {expression}'))==expected;assert number(ask(f'print-frame {thread} {sid} 0 {expression}'))==expected;record['values'].append({'expression':expression,'expected':expected,'passed':True})
 assert 'error:' in ask(f'print {thread} {sid+1000000} len ');ask(f'delete {bid}');assert 'guest exited:' not in ask(f'step wasm {thread}');assert 'error:' in ask(f'print {thread} {sid} cap ');ask('continue')
 for _ in range(64):
  exited=ask('wait')
  if 'guest exited:' in exited:assert 'guest exited: 0' in exited;break
 else:raise AssertionError('guest did not exit')
 record.update(passed=True,compiler_oracles=oracle,guest_completed_without_trap=True)
except BaseException as e:record.update(passed=False,error=repr(e));save();raise
finally:
 if console:
  try:
   console.finish();record.update(quit_returncode=console.child.returncode,managed_shutdown_complete=b'managed shutdown complete' in console.transcript);assert record['quit_returncode']==0 and record['managed_shutdown_complete']
  except BaseException as e:record.update(passed=False,shutdown_error=repr(e));save();raise
 for p,h in pins.items():
  if sha(p)!=h:record.update(passed=False,changed_input=p)
 save()
assert record['passed'];print('PASS real builtin-name/space/arithmetic C regression',a.policy,len(record['values']))

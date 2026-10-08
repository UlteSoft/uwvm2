#!/usr/bin/env python3
"""Actual standard gc Go Wasip1: metadata absence and safe failure, not parity."""
from __future__ import annotations
import argparse,json,re,subprocess,sys
from pathlib import Path
from run_debug_source_tinygo_cli import TinyGoConsole,named_function
import run_debug_source_inline_metadata_cli as meta

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for key in ('uwvm','wasm','objdump','out'):ap.add_argument('--'+key,type=Path,required=True)
 ap.add_argument('--ros',action='store_true');ap.add_argument('--full-policy',choices=['debug','legacy-light','pb-o1','pb-o2','pb-o3']);a=ap.parse_args();root=Path(__file__).resolve().parents[2]
 if sys.platform!='linux':raise RuntimeError('designated Linux cgroup only')
 subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);a.out.mkdir(parents=True,exist_ok=False)
 paths=[a.uwvm,a.wasm,a.objdump,Path(__file__),Path(meta.__file__),Path(__file__).with_name('run_debug_source_tinygo_cli.py')]
 pins={str(p):meta.sha(p) for p in paths};record={'passed':False,'language_level_status':'UNAVAILABLE','inputs_before':pins,'actions':[]};c=None
 try:
  text=a.objdump.read_text();assert '.debug_info' not in text and '.debug_line' not in text and 'go:buildid' in text and 'name' in text
  names=[meta.named_custom(data).decode() for tag,data in meta.sections(a.wasm) if tag==0]
  assert not any(n.startswith('.debug_') for n in names);record['actual_custom_sections']=names
  target,_=named_function(a.wasm,b'main.probeOuter');record['actual_guest_function']=target
  mode=['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full']
  argv=[str(a.uwvm),'-Rdbg',*mode,'-Rct','0','-Rllvm-call-stack','instruction','-Rllvm-cache-path','disable','--run',str(a.wasm)];record['argv']=argv
  if a.full_policy:argv[1:1]=['-Rllvm-full-policy',a.full_policy]
  record['full_policy_override']=a.full_policy
  c=TinyGoConsole(argv,a.out/'console.log')
  def ask(command):
   reply=c.send(command);record['actions'].append({'command':command,'reply':reply.decode(errors='strict')});assert len(c.transcript)<8*1024*1024;return reply
  assert b'prepared; no Wasm instruction executed' in ask('status')
  reply=ask('break-source 0 debug_source_tinygo.go:31');assert b'error:' in reply or b'rejected' in reply,reply
  reply=ask(f'break 0 {target} 0');m=re.search(rb'breakpoint (\d+)',reply);assert m is not None
  ask('continue')
  for _ in range(60):
   reply=ask('wait')
   if b'stopped: breakpoint' in reply:break
  else:raise AssertionError('real gc Go named Wasm activation did not stop')
  match=re.search(rb'thread (\d+) module=0 function=(\d+) byte-offset=',reply);assert match is not None and int(match[2])==target;thread=int(match[1])
  bt=ask(f'bt {thread}');assert b'main.probeOuter' in bt and b'\n  source ' not in bt;stop=re.search(rb'stop-id (\d+)',bt);assert stop is not None
  for command in (f'locals source {thread}',f'print {thread} {stop[1].decode()} value',f'step source {thread} over'):
   reply=ask(command);assert b'unavailable' in reply or b'error:' in reply or b'rejected' in reply,reply
   assert b'source-value stop=' not in reply
  after=ask(f'bt {thread}');assert re.search(rb'stop-id (\d+)',after)[1]==stop[1],'rejected source step changed actual stop'
  ask('delete '+m[1].decode());ask('continue')
  for _ in range(60):
   reply=ask('wait')
   if b'guest exited:' in reply:break
  else:raise AssertionError('actual gc Go guest did not exit')
  assert b'guest exited: 0' in reply,reply
  record.update(passed=True,reason='actual gc Wasip1 producer has no DWARF even with -N -l; source queries fail without fabricated values')
 except BaseException as error:record['error']=repr(error);raise
 finally:
  if c is not None:
   try:c.finish()
   except BaseException as error:record.update(passed=False,close_error=repr(error));raise
   finally:record.update(quit_returncode=c.child.returncode,managed_shutdown_complete=b'managed shutdown complete' in c.transcript)
  record['inputs_after']={p:meta.sha(Path(p)) for p in pins}
  if pins!=record['inputs_after']:record.update(passed=False,inputs_changed=True)
  (a.out/'summary.json').write_text(json.dumps(record,indent=2)+'\n')
 print('PASS actual gc Go metadata absence/stop guard/exit; language level UNAVAILABLE')

if __name__=='__main__':main()

#!/usr/bin/env python3
"""Check actual WASIp1 terminal unwind, exit code and physical managed shutdown.

Run in the Linux test cgroup with the adjacent nonzero or wide WAT fixture
compiled by wasm-tools. A private exit must unwind both real Wasm callers and
skip their unreachable instruction, while keeping the management console live.
"""
from pathlib import Path
import argparse,json,re,subprocess,sys
from run_debug_source_tinygo_cli import TinyGoConsole
from run_debug_source_inline_metadata_cli import sha

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--uwvm',type=Path,required=True);p.add_argument('--wasm',type=Path,required=True)
 p.add_argument('--code',type=int,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--ros',action='store_true')
 a=p.parse_args();root=Path(__file__).resolve().parents[2]
 if sys.platform!='linux' or not 0<=a.code<=0xffffffff:raise RuntimeError('require Linux cgroup and a u32 exit code')
 subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 binary=a.uwvm.resolve(strict=True);wasm=a.wasm.resolve(strict=True);out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
 cmd=[str(binary),'-Rdbg']+([] if a.ros else ['-Rcc','jit','-Rcm','full'])+['--wasip1-global-trace','out','-Rct','0','-Rllvm-call-stack','instruction','-Rllvm-cache-path','disable','--run',str(wasm)]
 command=[sys.executable,'-c','import os,sys,time;time.sleep(.15);os.execvpe(sys.argv[1],sys.argv[1:],os.environ)',*cmd]
 c=None;d={'passed':False,'binary_sha256':sha(binary),'wasm_sha256':sha(wasm),'expected_exit_code':a.code,'command':cmd,'actions':[]}
 def save():(out/'summary.json').write_text(json.dumps(d,indent=2))
 try:
  c=TinyGoConsole(command,out/'console.log')
  def ask(command):
   reply=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',c.send(command)).decode(errors='replace');d['actions'].append({'command':command,'reply':reply});save();return reply
  ask('continue');reply=''
  for _ in range(16):
   reply=ask('wait')
   if 'guest exited:' in reply:break
   if 'error:' in reply:raise AssertionError(reply)
  if not re.search(r'(?m)^guest exited: '+str(a.code)+r'$',reply):raise AssertionError(('wrong guest exit',reply))
  if 'guest exited: '+str(a.code) not in ask('status'):raise AssertionError('exit status was not retained')
  if not any('proc_exit('+str(a.code)+')' in item['reply'] for item in d['actions']):raise AssertionError('builtin exit trace was lost')
  d['passed']=True
 except BaseException as e:d['error']=repr(e);raise
 finally:
  if c:
   try:c.finish();d['quit_returncode']=c.child.returncode
   except BaseException as e:d['passed']=False;d['close_error']=repr(e);raise
   finally:d['managed_shutdown_complete']=b'managed shutdown complete' in c.transcript;save()
  save()

if __name__=='__main__':main()

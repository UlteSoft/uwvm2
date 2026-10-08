#!/usr/bin/env python3
"""Real VM replies routed through DAP over console transport; not socket authorization."""
import argparse,hashlib,importlib.util,io,json,resource,subprocess,sys,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for name in ('source-root','adapter-root','binary','wasm-tools','out'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();sys.path.insert(0,str(a.source_root/'test/0018.debugger'))
from run_wasm_operand_preview_cli import OperandConsole,markers
subprocess.run(['bash',str(a.source_root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_STACK,(64<<20,64<<20))
a.out.mkdir(parents=True,exist_ok=False)
spec=importlib.util.spec_from_file_location('live_deep_dap',a.adapter_root/'tools/debug/dap_adapter.py');dap=importlib.util.module_from_spec(spec);spec.loader.exec_module(dap)
wat=a.out/'deep.wat';wasm=wat.with_suffix('.wasm')
wat.write_text('(module (func $bottom (param i32) (result i64) i32.const -7 i64.const 123456789 nop drop drop i64.const 0) (func $recurse (param $n i32) (result i64) local.get $n if (result i64) local.get $n i64.const 123456789 local.get $n i32.const 1 i32.sub call $recurse drop drop drop i64.const 1 else local.get $n call $bottom end) (func (export "_start") i32.const 9997 call $recurse drop))\n')
subprocess.run([str(a.wasm_tools),'parse',str(wat),'-o',str(wasm)],check=True);subprocess.run([str(a.wasm_tools),'validate','--features','all',str(wasm)],check=True)
sites,_=markers(a.wasm_tools,wasm);results=[]
for policy in ('instruction','unwind'):
 c=OperandConsole([str(a.binary),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','-Rllvm-call-stack',policy,'--run',str(wasm)],a.out/(policy+'.log'))
 try:
  c.prompt();c.send(f'break 0 0 {sites[0][0]}');c.send('continue');deadline=time.monotonic()+30
  while b'stopped:' not in c.send('status'):assert time.monotonic()<deadline;time.sleep(.01)
  thread,_,_=c.location();output=io.BytesIO();adapter=dap.Adapter(output);adapter.step_level='wasm'
  class ActualConsole:
   def request(self,command):return c.send(command).removesuffix(b'(uwvm-debug) ').decode()
  adapter.broker=ActualConsole();sequence=0
  def send(command,**arguments):
   global sequence
   sequence+=1;output.seek(0);output.truncate();adapter.handle({'seq':sequence,'type':'request','command':command,'arguments':arguments})
   stream=io.BytesIO(output.getvalue());messages=[]
   while line:=stream.readline():
    assert stream.readline()==b'\r\n';messages.append(json.loads(stream.read(int(line[16:-2]))))
   responses=[m for m in messages if m['type']=='response'];assert len(responses)==1;return responses[0]
  reply=send('stackTrace',threadId=thread,startFrame=4096,levels=3);assert reply['success'],reply
  assert reply['body']['totalFrames']==10000 and len(reply['body']['stackFrames'])==3,reply
  frame=reply['body']['stackFrames'][0];assert adapter.frames[frame['id']][1]==4096
  scopes=send('scopes',frameId=frame['id']);assert scopes['success'],scopes
  ref=next(s['variablesReference'] for s in scopes['body']['scopes'] if s['name'].startswith('Wasm operand'))
  values=send('variables',variablesReference=ref);assert values['success'],values
  actual=[v['value'] for v in values['body']['variables']];assert '4095' in actual and '123456789' in actual,values
  end=send('stackTrace',threadId=thread,startFrame=10000,levels=3);assert end['success'] and end['body']=={'stackFrames':[],'totalFrames':10000},end
  step=send('stepIn',threadId=thread,granularity='instruction');assert step['success'],step
  retired=send('variables',variablesReference=ref);assert not retired['success'] and not adapter.frames,retired
  c.send('continue');deadline=time.monotonic()+30
  while b'guest exited: 0' not in c.send('status'):assert time.monotonic()<deadline;time.sleep(.01)
  results.append({'policy':policy,'actual_VM':True,'transport':'actual console replies','depth':10000,'frame':4096,'typed_values':actual,'retired_scope_rejected':True,'passed':True})
 finally:c.close()
(a.out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
(a.out/'inputs.json').write_text(json.dumps({'binary_sha256':hashlib.sha256(a.binary.read_bytes()).hexdigest(),'adapter_sha256':hashlib.sha256((a.adapter_root/'tools/debug/dap_adapter.py').read_bytes()).hexdigest(),'harness_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'cgroup':Path('/proc/self/cgroup').read_text()},indent=2)+'\n')
print(json.dumps(results),flush=True)

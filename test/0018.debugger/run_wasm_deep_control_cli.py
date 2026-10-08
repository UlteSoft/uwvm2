from pathlib import Path
import argparse,sys,subprocess,time,json,re,resource,hashlib,importlib.util
p=argparse.ArgumentParser()
for n in ('source-root','binary','wasm-tools','out'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--runner-prefix-json',type=Path,help='JSON argv prefix for the actual QEMU target VM')
p.add_argument('--prompt-timeout',type=int,default=120)
p.add_argument('--stop-timeout',type=int,default=30)
a=p.parse_args();sys.path.insert(0,str(a.source_root/'test/0018.debugger'))
from run_wasm_operand_preview_cli import OperandConsole,markers,operands
runner=json.loads(a.runner_prefix_json.read_text()) if a.runner_prefix_json else []
assert isinstance(runner,list) and all(isinstance(v,str) and v and '\0' not in v for v in runner)
assert 1 <= a.prompt_timeout <= 600 and 1 <= a.stop_timeout <= 120
OperandConsole.prompt_timeout=a.prompt_timeout
subprocess.run(['bash',str(a.source_root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_STACK,(64<<20,64<<20))
a.out.mkdir(parents=True,exist_ok=False)
spec=importlib.util.spec_from_file_location('deep_control_parser',a.source_root/'tools/debug/dap_adapter.py');dap=importlib.util.module_from_spec(spec);spec.loader.exec_module(dap)
cases={
 'deep-10000-eh':'''(module (tag $err (param i32))
   (func $bottom (param i32) (result i64) i32.const -9 nop throw $err)
   (func $recurse (param $n i32) (result i64) local.get $n if (result i64)
     local.get $n i32.const 1 i32.sub call $recurse else local.get $n call $bottom end)
   (func (export "_start") block $caught (result i32)
     try_table (catch $err $caught) i32.const 9997 call $recurse drop end unreachable end nop drop))''',
 'tail-20000':'''(module
   (func $tail (param $n i32) (result i64) local.get $n if (result i64)
     local.get $n i32.const 1 i32.sub return_call $tail else i64.const 37 nop end)
   (func (export "_start") i32.const 20000 call $tail nop drop))'''
}
results=[]
for name,wat_text in cases.items():
 wat=a.out/(name+'.wat');wasm=wat.with_suffix('.wasm');wat.write_text(wat_text+'\n')
 subprocess.run([str(a.wasm_tools),'parse',str(wat),'-o',str(wasm)],check=True);subprocess.run([str(a.wasm_tools),'validate','--features','all',str(wasm)],check=True)
 sites,dump=markers(a.wasm_tools,wasm);wat.with_suffix('.dump').write_text(dump)
 for policy in ('instruction','unwind'):
  row=dict(case=name,policy=policy,actual_VM=True,observations=[]);c=None
  try:
   c=OperandConsole([*runner,str(a.binary),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','-Rllvm-call-stack',policy,'-WFE-tail-call','-WFE-exceptions','--run',str(wasm)],a.out/(name+'-'+policy+'.log'));c.prompt()
   assert b'registered' in c.send(f'break 0 0 {sites[0][0]}');c.send('continue')
   limit=time.monotonic()+a.stop_timeout
   while b'stopped:' not in c.send('status'):assert time.monotonic()<limit;time.sleep(.01)
   thread,function,offset=c.location();assert function==0
   stop=dap.parse_status(c.send('status').removesuffix(b'(uwvm-debug) ').decode())[2][0]['stop_id']
   page=dap.parse_source_frames(c.send(f'frames wasm {thread} {stop} 4096 3').removesuffix(b'(uwvm-debug) ').decode(),thread,stop,with_page=True,wasm=True)
   assert page['total']==(10000 if name.startswith('deep') else 2),page
   operands(c,thread,0,['i32 = -9'] if name.startswith('deep') else ['i64 = 37'],row['observations'])
   c.send(f'step wasm {thread} out');limit=time.monotonic()+a.stop_timeout
   while b'stopped:' not in c.send('status'):assert time.monotonic()<limit;time.sleep(.01)
   assert c.location()[1]==(2 if name.startswith('deep') else 1)
   stop=dap.parse_status(c.send('status').removesuffix(b'(uwvm-debug) ').decode())[2][0]['stop_id']
   page=dap.parse_source_frames(c.send(f'frames wasm {thread} {stop} 0 3').removesuffix(b'(uwvm-debug) ').decode(),thread,stop,with_page=True,wasm=True)
   assert page['total']==1,page
   operands(c,thread,0,['i32 = -9'] if name.startswith('deep') else ['i64 = 37'],row['observations'])
   c.send('continue');limit=time.monotonic()+a.stop_timeout
   while True:
    reply=c.send('status')
    if b'guest exited:' in reply:assert b'guest exited: 0' in reply;break
    assert time.monotonic()<limit,reply;time.sleep(.01)
   row['passed']=True
  except Exception as e:row.update(passed=False,error=repr(e))
  finally:
   if c:c.close()
  results.append(row);(a.out/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps({k:v for k,v in row.items() if k!='observations'}),flush=True)
  if not row['passed']:raise AssertionError(row)
(a.out/'inputs.json').write_text(json.dumps(dict(binary_sha256=hashlib.sha256(a.binary.read_bytes()).hexdigest(),harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),runner_prefix=runner,runner_sha256=hashlib.sha256(Path(runner[0]).read_bytes()).hexdigest() if runner else None,prompt_timeout=a.prompt_timeout,stop_timeout=a.stop_timeout,cgroup=Path('/proc/self/cgroup').read_text()),indent=2)+'\n')

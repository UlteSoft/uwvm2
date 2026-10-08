#!/usr/bin/env python3
"""Real stopped Wasm replacement and fail-stop mutation script regression."""
import argparse,importlib.util,json,re,subprocess,time,hashlib
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source-root',type=Path,required=True);p.add_argument('--binary',type=Path,required=True)
p.add_argument('--wasm-tools',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
p.add_argument('--runner-prefix-json',type=Path,help='JSON argv prefix for QEMU user mode')
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
runner=json.loads(a.runner_prefix_json.read_text()) if a.runner_prefix_json else []
assert isinstance(runner,list) and all(isinstance(x,str) and x for x in runner)
subprocess.run(['bash',str(a.source_root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
spec=importlib.util.spec_from_file_location('step_cli',a.source_root/'test/0018.debugger/run_wasm_opcode_step_cli.py')
step=importlib.util.module_from_spec(spec);spec.loader.exec_module(step)
wat=a.out/'fixture.wat';wasm=a.out/'fixture.wasm'
wat.write_text('(module (memory 1) (func (result i32) i32.const 7) (func (export "_start") call 0 drop call 0 drop))\n')
subprocess.run([str(a.wasm_tools),'parse',str(wat),'-o',str(wasm)],check=True)
subprocess.run([str(a.wasm_tools),'validate','--features','all',str(wasm)],check=True)
for name,data in [('valid',b'\x00\x41\x09\x0b'),('wrong-type',b'\x00\x42\x09\x0b'),('bad-opcode',b'\x00\xff\x0b'),('whole-module',wasm.read_bytes())]:
 (a.out/(name+'.bin')).write_bytes(data)
rows=[]
for policy in ('instruction','unwind'):
 c=step.Console([*runner,str(a.binary),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','-Rllvm-call-stack',policy,'--run',str(wasm)],a.out/(policy+'.log'))
 row={'policy':policy,'actual_VM':True}
 def send(command):
  c.transcript.extend(b'\n>>> '+command.encode()+b'\n');c.child.stdin.write(command.encode()+b'\n');c.child.stdin.flush();return c.prompt()
 try:
  c.prompt();reply=send('wasm-script set wasm memory 0 0 1 0 bytes 7f;trace wasm on')
  assert b'wasm-script command 1' in reply and b'wasm-script command 2' not in reply,reply
  assert b'enabled=0' in send('trace wasm read')
  c.send('break 0 1 0');c.send('continue')
  deadline=time.monotonic()+20
  while b'stopped:' not in c.send('status'):assert time.monotonic()<deadline;time.sleep(.01)
  thread,func,offset=c.location();assert (func,offset)==(1,0)
  c.send('delete 1')
  reply=send(f'wasm-script set wasm memory 0 0 {thread} 0 bytes 7f;operands {thread}')
  assert b'applied=1' in reply and b'wasm-script command 2' in reply,reply
  assert b'7F' in c.send('memory 0 0 0 1').upper()
  reply=send(f'wasm-script set wasm memory 0 999 {thread} 0 bytes 7f;trace wasm on')
  assert b'applied=0' in reply and b'wasm-script command 2' not in reply,reply
  assert b'enabled=0' in c.send('trace wasm read')
  for bad in ('wrong-type','bad-opcode','whole-module'):
   reply=send(f'replace 0 0 1 {a.out/(bad+".bin")}')
   assert b'error:' in reply and b'function replaced' not in reply,(bad,reply)
  reply=c.send(f'wasm-script replace 0 0 1 {a.out/"valid.bin"};operands {thread}')
  assert b'function replaced; generation 2' in reply and b'wasm-script command 2' in reply,reply
  reply=send(f'replace 0 0 1 {a.out/"valid.bin"}');assert b'function generation changed' in reply,reply
  c.send('break 0 0 0');reply=c.send(f'step wasm {thread} over');assert b'breakpoint' in reply
  reply=send(f'replace 0 0 2 {a.out/"valid.bin"}');assert b'target is active' in reply,reply
  c.send('delete 2');c.send(f'step wasm {thread} out');assert c.location()[1:]==(1,2)
  assert b'i32 = 9' in c.send(f'operands {thread}')
  c.send('continue');deadline=time.monotonic()+20
  while True:
   reply=c.send('status')
   if b'guest exited:' in reply:assert b'guest exited: 0' in reply;break
   assert time.monotonic()<deadline;time.sleep(.01)
  row['passed']=True
 except Exception as error:row.update(passed=False,error=repr(error))
 finally:c.close()
 rows.append(row);(a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n');print(json.dumps(row),flush=True)
(a.out/'inputs.json').write_text(json.dumps(dict(binary_sha256=hashlib.sha256(a.binary.read_bytes()).hexdigest(),
 harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),runner_prefix=runner,
 runner_sha256=hashlib.sha256(Path(runner[0]).read_bytes()).hexdigest() if runner else None,
 cgroup=Path('/proc/self/cgroup').read_text()),indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in rows) else 1)

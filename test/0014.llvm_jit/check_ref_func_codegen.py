#!/usr/bin/env python3
"""Compare actual old/new full-JIT ref.func objects and verify cross-process cache rebinding."""
import argparse,hashlib,json,re,resource,subprocess
from pathlib import Path
from check_wasm3_native_frame_codegen import decode_object
from run_function_signature_cli import expand
p=argparse.ArgumentParser(description=__doc__)
for flag in ('uwvm','before-uwvm','wasm-tools','out'):p.add_argument('--'+flag,type=Path,required=True)
p.add_argument('--ros',action='store_true');a=p.parse_args();root=Path(__file__).resolve().parents[2]
subprocess.run(['bash',str(root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True);resource.setrlimit(resource.RLIMIT_CORE,(0,0));a.out.mkdir(parents=True,exist_ok=False)
provider='(module (func (export "f") (result i32) i32.const 42))'
consumer='''(module (type $t (func (result i32)))
(import "p" "f" (func $a (type $t))) (import "p" "f" (func $b (type $t)))
(table 3 funcref) (func $local (type $t) i32.const 7) (elem declare func $a $b $local)
(func $get_a (result funcref) ref.func $a ref.as_non_null)
(func $get_b (result funcref) ref.func $b ref.as_non_null)
(func $get_local (result funcref) ref.func $local ref.as_non_null)
(func (export "_start") i32.const 0 call $get_a table.set i32.const 1 call $get_b table.set i32.const 2 call $get_local table.set
 i32.const 0 call_indirect (type $t) i32.const 42 i32.ne if unreachable end
 i32.const 1 call_indirect (type $t) i32.const 42 i32.ne if unreachable end
 i32.const 2 call_indirect (type $t) i32.const 7 i32.ne if unreachable end))'''
for name,wat in [('provider',provider),('consumer',consumer)]:
 src=a.out/(name+'.wat');src.write_text(wat);path=a.out/(name+'.wasm');subprocess.run([str(a.wasm_tools),'parse',str(src),'-o',str(path)],check=True)
 if name=='consumer':
  # Exercise the new explicit function-result grammar too; the baseline supports this encoding.
  encoded=expand(path.read_bytes());path.write_bytes(encoded[0] if isinstance(encoded,tuple) else encoded)
rows=[]
for policy in ('instruction','unwind'):
 cache=a.out/(policy+'-cache');cache.mkdir()
 for version,binary in [('before',a.before_uwvm),('after',a.uwvm)]:
  out=a.out/(policy+'-'+version);out.mkdir();objects=[]
  for phase in ('cold','warm'):
   existing=set(cache.rglob('*.uwvm-ljc'));compile_log=out/(phase+'.compile.log')
   command=[str(binary)]+(['-Raot'] if a.ros else ['-Rcc','jit','-Rcm','full'])+['-Wpre',str(a.out/'provider.wasm'),'p','-WFE-function-references','-Rllvm-call-stack',policy,'-Rllvm-full-policy','pb-o3','-Rllvm-cache-path','path',str(cache),'-Rclog','file',str(compile_log),'--run',str(a.out/'consumer.wasm')]
   result=subprocess.run(command,capture_output=True,timeout=60);(out/(phase+'.log')).write_bytes(result.stdout+result.stderr);(out/(phase+'.command')).write_text(json.dumps(command)+'\n');assert result.returncode==0,result.stderr
   hits=compile_log.read_text().count('object-cache-hit');created=set(cache.rglob('*.uwvm-ljc'))-existing
   assert (hits>0)==(phase=='warm'),(version,phase,hits)
   if phase=='cold':assert created;objects=sorted(created)
   else:assert not created
  checked=[]
  for index,cached in enumerate(objects):
   obj=out/(str(index)+'.o');obj.write_bytes(decode_object(cached.read_bytes(),a.ros));assembly=subprocess.check_output(['/toolchain/bin/llvm-objdump','-dr',str(obj)],text=True);(out/(str(index)+'.s')).write_text(assembly)
   sections=subprocess.check_output(['/toolchain/bin/llvm-objdump','-h',str(obj)],text=True);assert '.eh_frame' in sections
   chunks=re.split(r'(?m)^([0-9a-f]+) <(.+)>:\n',assembly)
   for i in range(1,len(chunks),3):
    name,body=chunks[i+1:i+3]
    if not re.fullmatch(r'uwvm_m_[0-9a-f]+_func_[345]',name):continue
    calls=len(re.findall(r'\bcallq?\s',body));expected=2 if policy=='instruction' else 0
    if version=='after':assert calls==expected,(name,body)
    else:assert calls>expected,(name,body)
    checked.append(dict(function=name,calls=calls,text_lines=len(body.splitlines())))
  assert len(checked)==3,checked
  rows.append(dict(version=version,policy=policy,functions=checked,binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),cache_replay=True))
(a.out/'summary.json').write_text(json.dumps(dict(passed=True,rows=rows),indent=2)+'\n')
print('PASS actual ref.func code: zero construction/trap helper calls; 2 instruction-frame calls versus 0 unwind calls, cache isolation and rebinding')
